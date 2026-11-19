"""common/mdtest.c against tools/dev/mdtest_window.py, an independent
implementation: test_mdtest.c (run first by `make test`) writes the window it
filled to build/mdtest_window.bin."""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "dev"))
import mdtest_window  # noqa: E402

DUMP = os.path.join(HERE, "build", "mdtest_window.bin")


class WindowMatchesPython(unittest.TestCase):
    def setUp(self):
        if not os.path.exists(DUMP):
            self.fail(f"{DUMP} missing: run `make -C tests/host test`")
        data = open(DUMP, "rb").read()
        self.words = [(data[2 * i] << 8) | data[2 * i + 1] for i in range(len(data) // 2)]

    def test_every_word(self):
        expected = mdtest_window.window_words("v2.0.0", "d7ef4fe+debug")
        self.assertEqual(len(self.words), len(expected))
        diffs = [i for i, (a, b) in enumerate(zip(self.words, expected)) if a != b]
        self.assertEqual(diffs, [], f"first differing index {diffs[:1]}")

    def test_header_reads_back(self):
        h = mdtest_window.read_header(self.words)
        self.assertEqual(h["id"], "MDTESTWN")
        self.assertEqual(h["layout"], 1)
        self.assertEqual(h["state"], 0)
        self.assertTrue(h["checksum_ok"])
        self.assertEqual(h["version"], "v2.0.0")
        self.assertEqual(h["build"], "d7ef4fe+debug")

    def test_first_long_is_no_cartridge_magic(self):
        first = (self.words[0] << 16) | self.words[1]
        self.assertNotIn(first, (0xABCDEF42, 0xFA52235F))


if __name__ == "__main__":
    unittest.main()
