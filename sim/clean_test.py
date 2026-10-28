"""Safety checks for make clean, using temporary directories only."""
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import clean


class CleanTests(unittest.TestCase):
    def test_outputs_only_and_idempotent(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(clean, 'ROOT', Path(tmp)):
            root = Path(tmp)
            for relative in (*clean.TARGETS, 'sw/PvZ-simplized/assets', 'sw/mario/assets'):
                path = root / relative
                path.mkdir(parents=True, exist_ok=True)
                (path / 'example').touch()
            clean.clean()
            clean.clean()
            for relative in clean.TARGETS:
                self.assertFalse((root / relative).exists())
            self.assertTrue((root / 'sw/PvZ-simplized/assets/example').exists())
            self.assertTrue((root / 'sw/mario/assets/example').exists())

    def test_legacy_saves_refused(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(clean, 'ROOT', Path(tmp)):
            saved = Path(tmp) / 'build/pvz-native/userdata'
            saved.mkdir(parents=True)
            with self.assertRaises(SystemExit):
                clean.clean()
            self.assertTrue(saved.exists())

    def test_native_preferences_preserved(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(clean, 'ROOT', Path(tmp)):
            path = Path(tmp) / 'build/mario-native/smbc.conf'
            path.parent.mkdir(parents=True)
            path.write_bytes(b'custom user preferences\n')
            clean.clean()
            self.assertEqual(path.read_bytes(), b'custom user preferences\n')

    def test_symlink_refused_before_removal(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(clean, 'ROOT', Path(tmp)):
            root = Path(tmp)
            (root / 'build').mkdir()
            (root / 'elsewhere').mkdir()
            (root / 'sw').symlink_to(root / 'elsewhere', target_is_directory=True)
            with self.assertRaises(SystemExit):
                clean.clean()
            self.assertTrue((root / 'build').exists())


if __name__ == '__main__':
    unittest.main()
