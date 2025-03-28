#!/usr/bin/env python3

import os
import subprocess
from unittest import main, TestCase


class Tests(TestCase):
    """Run each provided test trace & compare to reference."""

    cwd = os.getcwd()
    tstd = cwd + "/test"

    drvr = "./sdriver.pl"
    itsh = "./tsh"
    rtsh = "./tshref"
    args = '"-p"'

    @classmethod
    def get_cmd(cls, number: int, impl: str) -> str:
        """Build sdriver command string for given trace number & tiny shell implementation path."""
        return f"{cls.drvr} -t trace{number:02d}.txt -s {impl} -a {cls.args}"

    @classmethod
    def run_test(cls, number: int, impl: str) -> str:
        """Run test & get output for given trace number & tiny shell implementation."""
        with subprocess.Popen(
            cls.get_cmd(number, impl),
            shell=True, # running in new shell seems to guarantee capture of all output
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, # combine stdout & stderr into one stream
        ) as pipe:
            if pipe.stdout is None:
                raise ValueError("Expected stdout, got None")

            with pipe.stdout as stdout:
                # decode each line to utf-8 from bytestring & join into one string
                output = "".join(
                                 [l.decode()
                                  for l in stdout.readlines() ])
                return output

    @classmethod
    def exec(cls, number: int) -> tuple[str, str]:
        """Run test & return outputs for given trace number using own shell & ref implementation."""
        act = cls.run_test(number, cls.itsh)
        exp = cls.run_test(number, cls.rtsh)

        return act, exp

    def assertMultilineEqualExceptPid(self, actual: str, expected: str) -> None:
        """Assert two multiline strings are equal, except for known locations of PID values."""
        act_lines = actual.split("\n")
        exp_lines = expected.split("\n")

        if not (len(act_lines) == len(exp_lines)):
            # when not the same number of lines, use assertMultiLineEqual to
            # get a moer helpful failure message
            self.assertMultiLineEqual(actual, expected)
        else:
            # otherwise, compare line by line
            for idx, act_line in enumerate(act_lines):
                exp_line = exp_lines[idx]

                # when at line that starts w/ `[`, compare part before pid &
                # part after pid only as pid is expected to differ
                if (
                    act_line.startswith("[") or
                    act_line.startswith("Job [")
                ):
                    # NOTE: this assumes there's only ever one PID in a line
                    # and it will break if more than one (<pid>) is found
                    act_beg, act_rest = act_line.split("(")
                    _, act_end = act_rest.split(")")

                    exp_beg, exp_rest = exp_line.split("(")
                    _, exp_end = exp_rest.split(")")

                    self.assertEqual(
                            act_beg,
                            exp_beg,
                            f"failed to match before pid in line {idx} of:\n" +
                            "actual\n" +
                            "--------------------\n" +
                            f"{actual}\n" +
                            "~~~~~~~~~~~~~~~~~~~~\n" +
                            "expected\n" +
                            "--------------------\n" +
                            f"{expected}")
                    self.assertEqual(
                            act_end,
                            exp_end,
                            f"failed to match after pid in line {idx} of:\n" +
                            "actual\n" +
                            "--------------------\n" +
                            f"{actual}\n" +
                            "~~~~~~~~~~~~~~~~~~~~\n" +
                            "expected\n" +
                            "--------------------\n" +
                            f"{expected}")
                # otherwise just compare lines directly
                else:
                    self.assertEqual(
                            act_line,
                            exp_line,
                            f"failed to match in line {idx} of: \n" +
                            "actual\n" +
                            "--------------------\n" +
                            f"{actual}\n" +
                            "~~~~~~~~~~~~~~~~~~~~\n" +
                            "expected\n" +
                            "--------------------\n" +
                            f"{expected}")

    def test_trace01(self) -> None:
        act, exp = self.exec(1)
        self.assertMultiLineEqual(act, exp)

    def test_trace02(self) -> None:
        act, exp = self.exec(2)
        self.assertMultiLineEqual(act, exp)

    def test_trace03(self) -> None:
        act, exp = self.exec(3)
        self.assertMultiLineEqual(act, exp)

    def test_trace04(self) -> None:
        act, exp = self.exec(4)
        self.assertMultilineEqualExceptPid(act, exp)

    def test_trace05(self) -> None:
        act, exp = self.exec(5)
        self.assertMultilineEqualExceptPid(act, exp)

    def test_trace06(self) -> None:
        act, exp = self.exec(6)
        self.assertMultilineEqualExceptPid(act, exp)

    def test_trace07(self) -> None:
        act, exp = self.exec(7)
        self.assertMultilineEqualExceptPid(act, exp)


if __name__ == "__main__":
    main()
