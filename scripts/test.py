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
    def get_iout_path(cls, number: str) -> str:
        return cls.tstd + f"/iout{number}"

    @classmethod
    def get_rout_path(cls, number: str) -> str:
        return cls.tstd + f"/rout{number}"

    @classmethod
    def get_cmd(cls, number: int, impl: str) -> str:
        return f"{cls.drvr} -t trace{number:02d}.txt -s {impl} -a {cls.args}"

    @classmethod
    def run_test(cls, number: int, impl: str) -> str:
        return subprocess.run(
            cls.get_cmd(number, impl).split(" "), capture_output=True
        ).stdout.decode()

    @classmethod
    def exec(cls, number: int) -> tuple[str, str]:
        act = cls.run_test(number, cls.itsh)
        exp = cls.run_test(number, cls.rtsh)

        return act, exp

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
        act_lines = act.split("\n")
        exp_lines = exp.split("\n")

        if not (len(act_lines) == len(exp_lines)):
            # when not the same number of lines, use assertMultiLineEqual to
            # get a moer helpful failure message
            self.assertMultiLineEqual(act, exp)
        else:
            # otherwise, compare line by line
            for idx, act_line in enumerate(act_lines):
                exp_line = exp_lines[idx]

                # when at line that starts w/ `[`, compare part before pid &
                # part after pid only as pid is expected to differ
                if act_line.startswith("["):
                    act_beg, act_rest = act_line.split("(")
                    act_pid, act_end = act_rest.split(")")

                    exp_beg, exp_rest = exp_line.split("(")
                    exp_pid, exp_end = exp_rest.split(")")

                    self.assertEqual(act_beg, exp_beg)
                    self.assertEqual(act_end, exp_end)
                # otherwise just compare lines directly
                else:
                    self.assertEqual(act_line, exp_line)


if __name__ == "__main__":
    main()
