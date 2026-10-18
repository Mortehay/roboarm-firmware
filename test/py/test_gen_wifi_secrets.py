import os
import shutil
import subprocess
import sys
import tempfile
import unittest

def write(path, text):
    with open(path, "w") as f:
        f.write(text)


def read(path):
    with open(path) as f:
        return f.read()


ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
SCRIPT = os.path.join(ROOT, "scripts", "gen_wifi_secrets.py")

import gen_wifi_secrets as g  # noqa: E402

GOOD = "WIFI_SSID=Net\nWIFI_PASSWORD=password1\nARM_KEY=abcdefgh\n"


class ParseEnv(unittest.TestCase):
    def test_parse(self):
        self.assertEqual(
            g.parse_env('A=1\r\n# c\n\nB="x y"\nC=\'q\'\n'),
            {"A": "1", "B": "x y", "C": "q"},
        )

    def test_no_expansion(self):
        self.assertEqual(g.parse_env("A=$HOME\n"), {"A": "$HOME"})


class Validate(unittest.TestCase):
    def test_missing(self):
        errs = " ".join(g.validate({}))
        for k in ("WIFI_SSID", "WIFI_PASSWORD", "ARM_KEY"):
            self.assertIn(k, errs)

    def test_bad_key_value_not_leaked(self):
        errs = g.validate({"WIFI_SSID": "a", "WIFI_PASSWORD": "password1", "ARM_KEY": "bad key!"})
        self.assertTrue(any("ARM_KEY" in e for e in errs))
        self.assertFalse(any("bad key!" in e for e in errs))

    def test_short_password(self):
        errs = g.validate({"WIFI_SSID": "a", "WIFI_PASSWORD": "1234567", "ARM_KEY": "abcdefgh"})
        self.assertTrue(any("WIFI_PASSWORD" in e for e in errs))

    def test_ok(self):
        self.assertEqual(g.validate(g.parse_env(GOOD)), [])


@unittest.skipUnless(shutil.which("gcc"), "gcc missing")
class Render(unittest.TestCase):
    def test_compiles_and_matches(self):
        ssid, pw, key = 'Ten"da', "p\\w\x01xxxxxx", "abcdefgh"
        hdr = g.render_header(ssid, pw, key)
        with tempfile.TemporaryDirectory() as d:
            write(os.path.join(d, "h.h"), hdr)
            c = (
                '#include <string.h>\n#include "h.h"\n'
                "int main(void){\n"
                '  if(strcmp(ROBOARM_WIFI_SSID,"Ten\\"da"))return 1;\n'
                '  if(strcmp(ROBOARM_WIFI_PASSWORD,"p\\\\w\\x01" "xxxxxx"))return 2;\n'
                '  if(strcmp(ROBOARM_ARM_KEY,"abcdefgh"))return 3;\n'
                "  return 0;}\n"
            )
            write(os.path.join(d, "t.c"), c)
            subprocess.run(["gcc", "-Wall", "-Werror", "-I", d, "-o", os.path.join(d, "t"), os.path.join(d, "t.c")], check=True)
            self.assertEqual(subprocess.run([os.path.join(d, "t")]).returncode, 0)

    def test_hex_escape_terminated(self):
        # a hex digit after \x01 must not be swallowed
        hdr = g.render_header("a", "\x01bcdefgh", "abcdefgh")
        with tempfile.TemporaryDirectory() as d:
            write(os.path.join(d, "h.h"), hdr)
            write(os.path.join(d, "t.c"),
                '#include <string.h>\n#include "h.h"\n'
                'int main(void){return strcmp(ROBOARM_WIFI_PASSWORD,"\\x01" "bcdefgh")!=0;}\n'
            )
            subprocess.run(["gcc", "-I", d, "-o", os.path.join(d, "t"), os.path.join(d, "t.c")], check=True)
            self.assertEqual(subprocess.run([os.path.join(d, "t")]).returncode, 0)


    def test_trigraph_c99(self):
        pw = "ab??/cd??=ef"
        hdr = g.render_header("a", pw, "abcdefgh")
        with tempfile.TemporaryDirectory() as d:
            write(os.path.join(d, "h.h"), hdr)
            write(os.path.join(d, "t.c"),
                  '#include <string.h>\n#include "h.h"\n'
                  'int main(void){return strcmp(ROBOARM_WIFI_PASSWORD,"ab\\?\\?/cd\\?\\?=ef")!=0;}\n')
            subprocess.run(["gcc", "-std=c99", "-Wall", "-Werror", "-I", d, "-o", os.path.join(d, "t"), os.path.join(d, "t.c")], check=True)
            self.assertEqual(subprocess.run([os.path.join(d, "t")]).returncode, 0)


class Cli(unittest.TestCase):
    def run_cli(self, env, out):
        return subprocess.run([sys.executable, SCRIPT, env, out], capture_output=True, text=True)

    def test_write_and_idempotent(self):
        with tempfile.TemporaryDirectory() as d:
            env, out = os.path.join(d, ".env"), os.path.join(d, "w.h")
            write(env, GOOD)
            self.assertEqual(self.run_cli(env, out).returncode, 0)
            self.assertIn("ROBOARM_WIFI_SSID", read(out))
            os.utime(out, (1000, 1000))
            self.assertEqual(self.run_cli(env, out).returncode, 0)
            self.assertEqual(os.stat(out).st_mtime, 1000)

    def test_mode_0600(self):
        with tempfile.TemporaryDirectory() as d:
            env, out = os.path.join(d, ".env"), os.path.join(d, "w.h")
            write(env, GOOD)
            self.run_cli(env, out)
            self.assertEqual(os.stat(out).st_mode & 0o777, 0o600)
            os.chmod(out, 0o644)
            self.run_cli(env, out)  # unchanged-content path
            self.assertEqual(os.stat(out).st_mode & 0o777, 0o600)
            self.assertEqual([f for f in os.listdir(d) if f.startswith(".wifi")], [])

    def test_invalid_utf8(self):
        with tempfile.TemporaryDirectory() as d:
            env, out = os.path.join(d, ".env"), os.path.join(d, "w.h")
            with open(env, "wb") as f:
                f.write(b"WIFI_SSID=\xff\xfe\n")
            r = self.run_cli(env, out)
            self.assertEqual(r.returncode, 1)
            self.assertTrue(r.stderr.startswith("error:"))
            self.assertNotIn("Traceback", r.stderr)
            self.assertFalse(os.path.exists(out))

    def test_unwritable_output(self):
        with tempfile.TemporaryDirectory() as d:
            env = os.path.join(d, ".env")
            write(env, GOOD)
            r = self.run_cli(env, os.path.join(d, "no", "such", "w.h"))
            self.assertEqual(r.returncode, 1)
            self.assertTrue(r.stderr.startswith("error:"))
            self.assertNotIn("Traceback", r.stderr)
            self.assertNotIn("password1", r.stderr)

    def test_lone_cr_not_a_separator(self):
        self.assertEqual(g.parse_env("A=1\rB=2\n"), {"A": "1\rB=2"})

    def test_leak_distinctive(self):
        with tempfile.TemporaryDirectory() as d:
            env, out = os.path.join(d, ".env"), os.path.join(d, "w.h")
            write(env, "WIFI_SSID=Net\nWIFI_PASSWORD=Zq9xyz\nARM_KEY=bad key!Zq9xyz\n")
            r = self.run_cli(env, out)
            self.assertEqual(r.returncode, 1)
            self.assertNotIn("Zq9xyz", r.stdout + r.stderr)

    def test_invalid(self):
        with tempfile.TemporaryDirectory() as d:
            env, out = os.path.join(d, ".env"), os.path.join(d, "w.h")
            write(env, "WIFI_SSID=Net\nWIFI_PASSWORD=short\nARM_KEY=abcdefgh\n")
            r = self.run_cli(env, out)
            self.assertEqual(r.returncode, 1)
            self.assertIn("WIFI_PASSWORD", r.stderr)
            self.assertNotIn("short\n", r.stderr.replace("WIFI_PASSWORD", ""))
            self.assertFalse(os.path.exists(out))

    def test_missing_env_file(self):
        with tempfile.TemporaryDirectory() as d:
            r = self.run_cli(os.path.join(d, "nope"), os.path.join(d, "w.h"))
            self.assertEqual(r.returncode, 1)


if __name__ == "__main__":
    unittest.main()
