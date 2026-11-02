"""Tests for explicit mysqltest selection; no database process is started here."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[4]
SPEC = importlib.util.spec_from_file_location(
    "seekdb_mysqltest_runner", ROOT / ".github/script/seekdb/mysqltest_for_seekdb.py")
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


class SQLCaseSelectionTest(unittest.TestCase):
    def setUp(self):
        self.cases = [RUNNER.MysqltestCase(name, Path(name + ".test"), Path(name + ".result"))
                      for name in ("ordinary", "ai_function.ai_function_enhancement", "ai_function.ai_model_ddl")]

    def test_default_keeps_existing_selection(self):
        self.assertEqual(RUNNER.select_cases(self.cases, []), self.cases)

    def test_exact_selection_preserves_requested_order(self):
        names = ["ai_function.ai_model_ddl", "ai_function.ai_function_enhancement"]
        self.assertEqual([case.name for case in RUNNER.select_cases(self.cases, names)], names)

    def test_unknown_names_cannot_turn_into_successful_zero_test_runs(self):
        with self.assertRaisesRegex(RUNNER.RunnerError, "unknown or excluded"):
            RUNNER.select_cases(self.cases, ["ai_function.typo"])

    def test_duplicate_and_empty_selections_fail(self):
        with self.assertRaisesRegex(RUNNER.RunnerError, "duplicate"):
            RUNNER.select_cases(self.cases, ["ordinary", "ordinary"])
        with self.assertRaisesRegex(RUNNER.RunnerError, "no mysqltest"):
            RUNNER.select_cases([], [])

    def test_actual_official_acceptance_case_is_available(self):
        selected = RUNNER.select_cases(RUNNER.discover_cases(ROOT), ["ai_function.ai_function_enhancement"])
        self.assertEqual(len(selected), 1)
        self.assertTrue(selected[0].test_file.is_file())
        self.assertTrue(selected[0].result_file.is_file())

    def test_merge_parser_accepts_the_same_selection(self):
        args = RUNNER.create_parser().parse_args([
            "merge", "--results-dir", "/tmp/unused", "--slice-count", "1", "--output", "/tmp/unused.json",
            "--case", "ai_function.ai_function_enhancement"])
        self.assertEqual(args.cases, ["ai_function.ai_function_enhancement"])


if __name__ == "__main__":
    unittest.main()
