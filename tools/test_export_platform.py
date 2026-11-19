import unittest
from export_platform import condition, specialize


class ExportTests(unittest.TestCase):
    def test_nested_and_elif(self):
        source = ("#if PRJ_MOBILGENE_ENABLE == 1U\ntarget\n"
                  "#if OTHER\nkeep\n#endif\n"
                  "#elif OTHER\npc_other\n#else\npc\n#endif\n")
        self.assertEqual(specialize(source, 1), "target\n#if OTHER\nkeep\n#endif\n")
        self.assertEqual(specialize(source, 0), "#if OTHER\npc_other\n#else\npc\n#endif\n")

    def test_mixed_conditions(self):
        self.assertEqual(condition("(PRJ_MOBILGENE_ENABLE == 0U) && (HOST == 1U)", 1), 0)
        self.assertEqual(condition("(PRJ_MOBILGENE_ENABLE == 1U) || defined(IMPL)", 1), 1)
        self.assertEqual(condition("(PRJ_MOBILGENE_ENABLE == 0U) && !defined(IMPL)", 0), "!(defined(IMPL))")

    def test_unknown_then_selected(self):
        source = "#if OTHER\nx\n#elif PRJ_MOBILGENE_ENABLE\ny\n#else\nz\n#endif\n"
        self.assertEqual(specialize(source, 1), "#if OTHER\nx\n#else\ny\n#endif\n")

    def test_selector_definition_removed(self):
        source = "#ifndef PRJ_MOBILGENE_ENABLE\n#define PRJ_MOBILGENE_ENABLE (1U)\n#endif\n"
        self.assertEqual(specialize(source, 0), "")

    def test_multiline(self):
        source = "#if (PRJ_MOBILGENE_ENABLE == 0U) && \\\n (HOST == 1U)\nx\n#endif\n"
        self.assertEqual(specialize(source, 1), "")
        self.assertIn("HOST", specialize(source, 0))

    def test_fail_closed(self):
        with self.assertRaises(ValueError):
            specialize("#if PRJ_MOBILGENE_ENABLE + 1\nx\n#endif\n", 1)
        with self.assertRaises(ValueError):
            specialize("#if OTHER\nx\n", 0)


if __name__ == "__main__":
    unittest.main()
