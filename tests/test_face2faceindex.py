"""Exercise face2faceindex through its command-line interface.

Usage: python tests/test_face2faceindex.py path/to/face2faceindex.exe [-v]
Only the Python standard library is required. Generated files stay in a
temporary directory; the supplied handout models are read without modification.
"""

from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
EXECUTABLE = None


def read_triangle_soup(path):
    tokens = path.read_text(encoding="utf-8").split()
    count = int(tokens[0])
    coordinates = [float(token) for token in tokens[1:]]
    if len(coordinates) != count * 9:
        raise ValueError("Invalid triangle count in test fixture: " + str(path))
    points = [tuple(coordinates[i:i + 3])
              for i in range(0, len(coordinates), 3)]
    return count, points


class Face2FaceIndexTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="face index tests ")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def run_converter(self, source, destination=None):
        arguments = [str(source)]
        if destination is not None:
            arguments.append(str(destination))
        return self.run_arguments(arguments)

    def run_arguments(self, arguments):
        return subprocess.run([str(EXECUTABLE)] + arguments,
                              capture_output=True, text=True,
                              timeout=120, cwd=self.directory)

    def write_source(self, contents, name="input.tri"):
        path = self.directory / name
        path.write_text(contents, encoding="utf-8")
        return path

    def read_face(self, path, object_name):
        lines = path.read_text(encoding="utf-8").splitlines()
        vertices, faces, header = [], [], []
        data_started = False
        faces_started = False
        for line in lines:
            line = line.strip()
            if not line:
                continue
            if line.startswith("#"):
                self.assertFalse(data_started, "Header must precede both blocks")
                header.append(line)
                continue
            self.assertTrue(header, "A .face file must begin with a header")
            data_started = True
            fields = line.split()
            self.assertIn(fields[0], ("Vertex", "Face"))
            self.assertEqual(len(fields), 5)
            if fields[0] == "Vertex":
                self.assertFalse(faces_started, "Vertex block must precede Face block")
                self.assertEqual(int(fields[1]), len(vertices))
                vertices.append(tuple(float(value) for value in fields[2:]))
            else:
                faces_started = True
                self.assertEqual(int(fields[1]), len(faces))
                face = tuple(int(value) for value in fields[2:])
                for index in face:
                    self.assertGreaterEqual(index, 0)
                    self.assertLess(index, len(vertices))
                faces.append(face)

        self.assertTrue(header, "Even an empty mesh needs a header")
        self.assertIn("# Object Name: " + object_name, header)
        counts = [re.fullmatch(r"#\s*Vertices=(\d+)\s+Faces=(\d+)", line)
                  for line in header]
        counts = [match for match in counts if match is not None]
        self.assertEqual(len(counts), 1, "Expected one vertex/face count header")
        self.assertEqual(tuple(map(int, counts[0].groups())),
                         (len(vertices), len(faces)))
        return vertices, faces

    def assert_conversion_succeeds(self, source, destination):
        result = self.run_converter(source, destination)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(destination.is_file(), "Conversion did not create output")
        return self.read_face(destination, source.stem)

    def test_tetrahedron_has_expected_vertices_and_faces(self):
        source = PROJECT_ROOT / "handout_models" / "tetrahedron.tri"
        vertices, faces = self.assert_conversion_succeeds(
            source, self.directory / "tetrahedron.face")
        self.assertEqual(vertices, [(-1.0, -1.0, -1.0), (1.0, -1.0, 1.0),
                                    (-1.0, 1.0, 1.0), (1.0, 1.0, -1.0)])
        self.assertEqual(faces, [(0, 1, 2), (0, 2, 3), (0, 3, 1), (3, 2, 1)])

    def test_all_handout_models_preserve_geometry_and_order(self):
        sources = sorted((PROJECT_ROOT / "handout_models").glob("*.tri"))
        self.assertTrue(sources, "No handout models found")
        for source in sources:
            with self.subTest(model=source.name):
                if source.name == "hamish.tri":
                    # The distributed file declares 7895 faces but contains
                    # 7953. Verify rejection, then round-trip a corrected
                    # temporary copy without changing the original fixture.
                    tokens = source.read_text(encoding="utf-8").split()
                    self.assertEqual(int(tokens[0]), 7895)
                    self.assertEqual(len(tokens) - 1, 7953 * 9)
                    rejected_output = self.directory / "invalid_hamish.face"
                    result = self.run_converter(source, rejected_output)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertTrue(result.stderr.strip())
                    self.assertFalse(rejected_output.exists())
                    source = self.write_source(
                        "7953\n" + " ".join(tokens[1:]) + "\n", "hamish.tri")
                count, original_points = read_triangle_soup(source)
                vertices, faces = self.assert_conversion_succeeds(
                    source, self.directory / (source.stem + ".face"))
                self.assertEqual(len(faces), count)
                reconstructed = [vertices[index] for face in faces for index in face]
                self.assertEqual(reconstructed, original_points)
                self.assertEqual(vertices, list(dict.fromkeys(original_points)),
                                 "Unique vertices must follow first occurrence")

    def test_numeric_equivalence_and_signed_zero_share_indices(self):
        source = self.write_source(
            "2\n"
            "0 -0.0 0e2   1 2 3   4 5 6\n"
            "-0 +0 0.000   1.0 2e0 3.000   4.0 5e0 6.000\n")
        vertices, faces = self.assert_conversion_succeeds(
            source, self.directory / "equivalent.face")
        self.assertEqual(vertices, [(0.0, 0.0, 0.0), (1.0, 2.0, 3.0),
                                    (4.0, 5.0, 6.0)])
        self.assertEqual(faces, [(0, 1, 2), (0, 1, 2)])

    def test_distinct_nearby_coordinates_and_precision_are_preserved(self):
        source = self.write_source(
            "1\n"
            "10.000000000000002 20.123456789012344 30.333333333333332\n"
            "10.000000000000004 20.123456789012344 30.333333333333332\n"
            "11.234567890123456 21.234567890123456 31.234567890123456\n")
        _, original_points = read_triangle_soup(source)
        vertices, faces = self.assert_conversion_succeeds(
            source, self.directory / "precision.face")
        self.assertEqual(vertices, original_points)
        self.assertEqual(faces, [(0, 1, 2)])

    def test_default_output_replaces_extension_in_path_with_spaces(self):
        folder = self.directory / "folder with spaces"
        folder.mkdir()
        source = folder / "mesh model.v1.tri"
        source.write_text("1\n10 20 30 11 20 30 10 21 30\n", encoding="utf-8")
        result = self.run_converter(source)
        self.assertEqual(result.returncode, 0, result.stderr)
        vertices, faces = self.read_face(source.with_suffix(".face"), source.stem)
        self.assertEqual(vertices, [(10.0, 20.0, 30.0), (11.0, 20.0, 30.0),
                                    (10.0, 21.0, 30.0)])
        self.assertEqual(faces, [(0, 1, 2)])

    def test_empty_mesh_is_valid(self):
        source = self.write_source("0\n\n")
        vertices, faces = self.assert_conversion_succeeds(
            source, self.directory / "empty.face")
        self.assertEqual(vertices, [])
        self.assertEqual(faces, [])

    def test_invalid_input_is_rejected_before_creating_output(self):
        cases = {
            "empty": "",
            "whitespace_only": " \n\t",
            "missing_coordinates": "1\n",
            "incomplete_vertex": "1\n0 0 0 1 0 0 0 1\n",
            "too_few_faces": "2\n0 0 0 1 0 0 0 1 0\n",
            "extra_coordinate": "1\n0 0 0 1 0 0 0 1 0 4\n",
            "trailing_text": "1\n0 0 0 1 0 0 0 1 0 garbage\n",
            "data_after_empty_mesh": "0\n0 0 0\n",
            "negative_count": "-1\n",
            "fractional_count": "1.5\n0 0 0 1 0 0 0 1 0\n",
            "exponent_count": "1e0\n0 0 0 1 0 0 0 1 0\n",
            "nonnumeric_count": "one\n",
            "overflow_count": "18446744073709551616\n",
            "nonnumeric_coordinate": "1\n0 0 0 1 0 0 0 1 invalid\n",
            "partial_coordinate": "1\n0 0 0 1 0 0 0 1 2x\n",
            "missing_coordinate_separator": "1\n0-1 0\n1 0 0\n0 1 0\n",
        }
        for token in ("nan", "NaN", "inf", "-inf", "infinity", "1e999"):
            cases["nonfinite_" + token] = "1\n0 0 0 1 0 0 0 1 " + token + "\n"
        for name, contents in cases.items():
            with self.subTest(case=name):
                source = self.write_source(contents, name + ".tri")
                destination = self.directory / (name + ".face")
                result = self.run_converter(source, destination)
                self.assertNotEqual(result.returncode, 0)
                self.assertTrue(result.stderr.strip(), "Failure needs a diagnostic")
                self.assertFalse(destination.exists(), "Invalid input created output")

    def test_missing_input_is_rejected_without_creating_output(self):
        source = self.directory / "does not exist.tri"
        destination = self.directory / "missing.face"
        result = self.run_converter(source, destination)
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(result.stderr.strip())
        self.assertFalse(destination.exists())

    def test_existing_output_is_rejected_without_modification(self):
        source = self.write_source("1\n0 0 0 1 0 0 0 1 0\n")
        destination = self.directory / "existing.face"
        original = b"Existing output that must survive conversion.\r\n"
        destination.write_bytes(original)
        result = self.run_converter(source, destination)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("already exists", result.stderr)
        self.assertEqual(destination.read_bytes(), original)

    def test_input_path_alias_cannot_overwrite_source(self):
        source = self.write_source("1\n0 0 0 1 0 0 0 1 0\n")
        original = source.read_bytes()
        # source is absolute, while subprocess cwd makes this different path
        # spelling name the same file. Preserve the literal './' in the CLI.
        result = self.run_converter(source, "./input.tri")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("already exists", result.stderr)
        self.assertEqual(source.read_bytes(), original)

    def test_invalid_input_does_not_truncate_existing_output(self):
        source = self.write_source("1\n0 0\n")
        destination = self.directory / "existing.face"
        original = b"Existing output must survive invalid input.\r\n"
        destination.write_bytes(original)
        result = self.run_converter(source, destination)
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(result.stderr.strip())
        self.assertEqual(destination.read_bytes(), original)

    def test_missing_output_parent_reports_failure(self):
        source = self.write_source("1\n0 0 0 1 0 0 0 1 0\n")
        destination = self.directory / "missing directory" / "output.face"
        result = self.run_converter(source, destination)
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(result.stderr.strip())
        self.assertFalse(destination.exists())

    def test_cli_argument_errors_and_help(self):
        source = self.write_source("1\n0 0 0 1 0 0 0 1 0\n")
        destination = self.directory / "unused.face"
        for arguments in ([], [str(source), str(destination), "extra"]):
            with self.subTest(arguments=arguments):
                result = self.run_arguments(arguments)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Usage:", result.stderr)
                self.assertFalse(destination.exists())
        result = self.run_arguments(["--help"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Usage:", result.stdout)
        self.assertIn("input.tri", result.stdout)
        self.assertIn("output.face", result.stdout)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit("Usage: python tests/test_face2faceindex.py "
                         "path/to/face2faceindex.exe [-v]")
    EXECUTABLE = Path(sys.argv.pop(1)).resolve()
    if not EXECUTABLE.is_file():
        raise SystemExit("Executable not found: " + str(EXECUTABLE))
    unittest.main()
