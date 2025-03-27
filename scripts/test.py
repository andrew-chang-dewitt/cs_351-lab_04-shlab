#!/usr/bin/env python3

import difflib
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

    def test_traces(self) -> None:
        for i in range(1, 3):
            with self.subTest(i=i):
                act = self.run_test(i, self.itsh)
                exp = self.run_test(i, self.rtsh)

                self.assertMultiLineEqual(act, exp)


if __name__ == "__main__":
    main()
