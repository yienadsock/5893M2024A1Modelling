"""CLI regression tests for Appendix 2; all generated data is temporary.

python tests/test_faceindex2directededge.py step2.exe --face2faceindex step1.exe -v
"""

import argparse
from collections import defaultdict
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
EXECUTABLE = None
STEP_ONE = None


class DirectedEdgeTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="directed edge tests ")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def run_tool(self, arguments, executable=None):
        return subprocess.run([str(executable or EXECUTABLE)]
                              + [str(argument) for argument in arguments],
                              cwd=self.directory, capture_output=True, text=True,
                              timeout=60)

    def source(self, text, name="mesh.face"):
        path = self.directory / name
        path.write_text(text, encoding="utf-8")
        return path

    def face_text(self, vertices, faces):
        lines = ["# A test author", "# Object Name: Mesh with spaces",
                 "# Vertices={} Faces={}".format(len(vertices), len(faces)), "#"]
        lines += ["Vertex {} {} {} {}".format(index, *vertex)
                  for index, vertex in enumerate(vertices)]
        lines += ["Face {} {} {} {}".format(index, *face)
                  for index, face in enumerate(faces)]
        return "\n".join(lines) + "\n"

    def parse_directed(self, text):
        names = ("Vertex", "FirstDirectedEdge", "Face", "OtherHalf")
        records = {name: [] for name in names}
        header = []
        current_block = -1
        for line in text.splitlines():
            stripped = line.strip()
            if not stripped:
                continue
            if stripped.startswith("#"):
                self.assertEqual(current_block, -1, "Header must come first")
                header.append(line)
                continue
            fields = stripped.split()
            self.assertIn(fields[0], names)
            block = names.index(fields[0])
            self.assertGreaterEqual(block, current_block, "Incorrect block order")
            current_block = block
            destination = records[fields[0]]
            self.assertEqual(int(fields[1]), len(destination))
            expected_length = 5 if fields[0] in ("Vertex", "Face") else 3
            self.assertEqual(len(fields), expected_length)
            parser = float if fields[0] == "Vertex" else int
            destination.append(tuple(parser(value) for value in fields[2:]))
        self.assertTrue(header)
        vertices = records["Vertex"]
        faces = records["Face"]
        first = [record[0] for record in records["FirstDirectedEdge"]]
        other = [record[0] for record in records["OtherHalf"]]
        self.assertEqual(len(first), len(vertices))
        self.assertEqual(len(other), 3 * len(faces))
        return header, vertices, faces, first, other

    def assert_connectivity(self, parsed):
        _, vertices, faces, first, other = parsed
        edges = []
        outgoing = defaultdict(list)
        incidence = defaultdict(list)
        degenerate = set()
        for face_id, face in enumerate(faces):
            self.assertTrue(all(0 <= vertex < len(vertices) for vertex in face))
            if len(set(face)) != 3:
                degenerate.add(face_id)
            a, b, c = face
            for origin, target in ((c, a), (a, b), (b, c)):
                edge_id = len(edges)
                edges.append((origin, target))
                outgoing[origin].append(edge_id)
                incidence[tuple(sorted((origin, target)))].append(edge_id)
        for vertex_id, edge_id in enumerate(first):
            self.assertEqual(edge_id, min(outgoing[vertex_id], default=-1))
        for edge_id, opposite in enumerate(other):
            self.assertTrue(opposite == -1 or 0 <= opposite < len(edges))
            if opposite != -1:
                self.assertNotEqual(opposite, edge_id)
                self.assertEqual(other[opposite], edge_id)
                self.assertEqual(edges[opposite], edges[edge_id][::-1])
                self.assertNotEqual(opposite // 3, edge_id // 3)
        for group in incidence.values():
            pairable = (len(group) == 2
                        and all(edge // 3 not in degenerate for edge in group)
                        and group[0] // 3 != group[1] // 3
                        and edges[group[0]] == edges[group[1]][::-1])
            if pairable:
                self.assertEqual(other[group[0]], group[1])
                self.assertEqual(other[group[1]], group[0])
            else:
                self.assertTrue(all(other[edge] == -1 for edge in group))

    def convert(self, source, destination=None):
        if destination is None:
            destination = source.with_suffix(".diredge")
            arguments = [source]
        else:
            arguments = [source, destination]
        result = self.run_tool(arguments)
        self.assertEqual(result.returncode, 0, result.stderr)
        parsed = self.parse_directed(destination.read_text(encoding="utf-8"))
        self.assert_connectivity(parsed)
        original_header = [line for line in source.read_text(encoding="utf-8").splitlines()
                           if line.lstrip().startswith("#")]
        self.assertEqual(parsed[0], original_header)
        return parsed, result

    def test_appendix_cube_matches_every_reference_edge(self):
        appendix = (PROJECT_ROOT / "appendix.txt").read_text(encoding="utf-8")
        first_part, second_part = appendix.split("APPENDIX 2:", 1)
        first_part = first_part[first_part.index("# University"):]
        second_part = second_part[second_part.index("# University"):]
        face_lines = [line for line in first_part.splitlines()
                      if line.startswith(("#", "Vertex ", "Face "))]
        reference_lines = [line for line in second_part.splitlines()
                           if line.startswith(("#", "Vertex ", "Face ",
                                               "FirstDirectedEdge ", "OtherHalf "))]
        parsed, result = self.convert(self.source("\n".join(face_lines) + "\n"))
        reference = self.parse_directed("\n".join(reference_lines))
        self.assertEqual(parsed[1:], reference[1:])
        self.assertEqual(parsed[3], [1, 2, 0, 3, 8, 6, 12, 23])
        self.assertEqual(len(parsed[4]), 36)
        self.assertEqual(result.stderr, "")

    def test_single_triangle_has_three_boundaries(self):
        parsed, result = self.convert(self.source(self.face_text(
            [(0, 0, 0), (1, 0, 0), (0, 1, 0)], [(0, 1, 2)])))
        self.assertEqual(parsed[3], [1, 2, 0])
        self.assertEqual(parsed[4], [-1, -1, -1])
        self.assertIn("boundary", result.stderr)

    def test_two_triangles_pair_the_shared_reverse_edge(self):
        parsed, _ = self.convert(self.source(self.face_text(
            [(0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 1, 0)],
            [(0, 1, 2), (0, 2, 3)])))
        self.assertEqual(parsed[4], [4, -1, -1, -1, 0, -1])

    def test_same_direction_edges_are_not_paired(self):
        parsed, result = self.convert(self.source(self.face_text(
            [(0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 1, 0)],
            [(0, 1, 2), (0, 1, 3)])))
        self.assertEqual(parsed[4], [-1] * 6)
        self.assertIn("orientation", result.stderr)

    def test_three_faces_on_one_edge_retain_every_incidence(self):
        parsed, result = self.convert(self.source(self.face_text(
            [(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1)],
            [(0, 1, 2), (1, 0, 3), (0, 1, 4)])))
        self.assertEqual(parsed[4], [-1] * 9)
        self.assertIn("non-manifold", result.stderr)
        self.assertIn("1 4 7", result.stderr)

    def test_repeated_vertex_face_does_not_pair_with_itself(self):
        parsed, result = self.convert(self.source(self.face_text(
            [(0, 0, 0), (1, 0, 0)], [(0, 0, 1)])))
        self.assertEqual(parsed[2], [(0, 0, 1)])
        self.assertEqual(parsed[4], [-1] * 3)
        self.assertIn("repeated vertex", result.stderr)

    def test_empty_mesh(self):
        parsed, result = self.convert(self.source(self.face_text([], [])))
        self.assertEqual(parsed[1:], ([], [], [], []))
        self.assertEqual(result.stderr, "")

    def test_isolated_vertex_has_no_first_edge(self):
        parsed, result = self.convert(self.source(self.face_text([(10, 20, 30)], [])))
        self.assertEqual(parsed[3], [-1])
        self.assertEqual(parsed[4], [])
        self.assertIn("isolated", result.stderr)

    def test_existing_indices_and_double_precision_are_preserved(self):
        vertices = [(10.000000000000002, 20.123456789012344, 30.333333333333332),
                    (10.000000000000004, 20.123456789012344, 30.333333333333332),
                    (11, 21, 31), (10.000000000000002, 20.123456789012344,
                                   30.333333333333332)]
        faces = [(0, 1, 2), (3, 2, 1)]
        parsed, _ = self.convert(self.source(self.face_text(vertices, faces)))
        self.assertEqual(parsed[1], vertices)
        self.assertEqual(parsed[2], faces)
        self.assertEqual(len(parsed[1]), 4, "Do not deduplicate .face vertices again")

    def test_crlf_blank_lines_and_spaced_default_output_path(self):
        text = self.face_text([(0, 0, 0), (1, 0, 0), (0, 1, 0)], [(0, 1, 2)])
        source = self.source(text.replace("\n", "\r\n\r\n"), "mesh version.1.face")
        self.convert(source)
        self.assertTrue((self.directory / "mesh version.1.diredge").is_file())

    def test_malformed_input_is_rejected_without_output(self):
        valid = self.face_text([(0, 0, 0), (1, 0, 0), (0, 1, 0)], [(0, 1, 2)])
        cases = {
            "empty": "",
            "no_header": "Vertex 0 0 0 0\n",
            "missing_count": "# Object Name: X\n",
            "duplicate_count": "# Vertices=0 Faces=0\n# Vertices=0 Faces=0\n",
            "wrong_vertex_count": valid.replace("Vertices=3", "Vertices=4"),
            "wrong_face_count": valid.replace("Faces=1", "Faces=2"),
            "negative_count": valid.replace("Vertices=3", "Vertices=-3"),
            "decimal_count": valid.replace("Vertices=3", "Vertices=3.0"),
            "overflow_count": valid.replace("Vertices=3", "Vertices=9999999999999999999999999"),
            "vertex_id_gap": valid.replace("Vertex 1 ", "Vertex 8 "),
            "duplicate_vertex_id": valid.replace("Vertex 1 ", "Vertex 0 "),
            "face_id_gap": valid.replace("Face 0 ", "Face 2 "),
            "negative_index": valid.replace("Face 0 0 1 2", "Face 0 -1 1 2"),
            "decimal_index": valid.replace("Face 0 0 1 2", "Face 0 0.0 1 2"),
            "out_of_range_index": valid.replace("Face 0 0 1 2", "Face 0 0 1 3"),
            "extra_field": valid.replace("Face 0 0 1 2", "Face 0 0 1 2 extra"),
            "missing_field": valid.replace("Face 0 0 1 2", "Face 0 0 1"),
            "unknown_record": valid + "OtherHalf 0 -1\n",
            "comment_after_data": valid + "# misplaced comment\n",
            "vertex_after_face": valid + "Vertex 3 9 9 9\n",
            "face_before_vertex": "# Vertices=1 Faces=1\nFace 0 0 0 0\nVertex 0 0 0 0\n",
        }
        for token in ("nan", "inf", "-inf", "1e999", "0-1", "1x"):
            cases["bad_coordinate_" + token] = valid.replace("Vertex 0 0 0 0",
                                                            "Vertex 0 " + token + " 0 0")
        for name, text in cases.items():
            with self.subTest(case=name):
                source = self.source(text, name + ".face")
                destination = self.directory / (name + ".diredge")
                result = self.run_tool([source, destination])
                self.assertNotEqual(result.returncode, 0)
                self.assertTrue(result.stderr.strip())
                self.assertFalse(destination.exists())

    def test_existing_output_and_source_alias_are_not_overwritten(self):
        source = self.source(self.face_text([], []))
        destination = self.directory / "existing.diredge"
        destination.write_bytes(b"KEEP OUTPUT\r\n")
        before = source.read_bytes()
        result = self.run_tool([source, destination])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("already exists", result.stderr)
        self.assertEqual(destination.read_bytes(), b"KEEP OUTPUT\r\n")
        result = self.run_tool([source, "./mesh.face"])
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(source.read_bytes(), before)
        malformed = self.source("INVALID", "invalid.face")
        result = self.run_tool([malformed, destination])
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(destination.read_bytes(), b"KEEP OUTPUT\r\n")

    def test_file_errors_arguments_and_help(self):
        source = self.source(self.face_text([], []))
        for arguments in ([], [source, "a", "b"], ["missing.face"],
                          [source, self.directory / "missing" / "out.diredge"]):
            with self.subTest(arguments=arguments):
                result = self.run_tool(arguments)
                self.assertNotEqual(result.returncode, 0)
                self.assertTrue(result.stderr.strip())
        for option in ("--help", "-h"):
            result = self.run_tool([option])
            self.assertEqual(result.returncode, 0)
            self.assertIn("Usage:", result.stdout)

    def test_all_models_through_both_command_line_tools(self):
        if STEP_ONE is None:
            self.skipTest("Pass --face2faceindex to test the complete pipeline")
        models = sorted((PROJECT_ROOT / "handout_models").glob("*.tri"))
        self.assertEqual(len(models), 25)
        for model in models:
            with self.subTest(model=model.name):
                tokens = model.read_text(encoding="utf-8").split()
                if model.name == "hamish.tri":
                    self.assertEqual(int(tokens[0]), 7895)
                    self.assertEqual(len(tokens) - 1, 7953 * 9)
                    tokens[0] = "7953"
                    model = self.source(" ".join(tokens), "hamish.tri")
                self.assertEqual(len(tokens) - 1, int(tokens[0]) * 9)
                original = [tuple(map(float, tokens[i:i + 3]))
                            for i in range(1, len(tokens), 3)]
                indexed = self.directory / (model.stem + ".face")
                result = self.run_tool([model, indexed], executable=STEP_ONE)
                self.assertEqual(result.returncode, 0, result.stderr)
                parsed, _ = self.convert(indexed)
                _, vertices, faces, _, _ = parsed
                self.assertEqual(len(faces), int(tokens[0]))
                self.assertEqual([vertices[index] for face in faces for index in face],
                                 original)
                if model.name == "tetrahedron.tri":
                    self.assertEqual(len(vertices), 4)
                    self.assertEqual(len(parsed[4]), 12)
                    self.assertNotIn(-1, parsed[4])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--face2faceindex", type=Path)
    arguments, unittest_arguments = parser.parse_known_args()
    EXECUTABLE = arguments.executable.resolve()
    STEP_ONE = arguments.face2faceindex.resolve() if arguments.face2faceindex else None
    for executable in (EXECUTABLE, STEP_ONE):
        if executable is not None and not executable.is_file():
            parser.error("Executable not found: " + str(executable))
    unittest.main(argv=[sys.argv[0]] + unittest_arguments)
