#!/usr/bin/env python3

import asyncio
import re
import subprocess
from typing import Generator, NewType, Optional, TypeGuard
from unittest import main, IsolatedAsyncioTestCase

type Match = tuple[re.Match[str], re.Match[str]] # shorthand for tuple of match groups
type MaybeMatchGen = Generator[tuple[Optional[re.Match[str]], Optional[re.Match[str]]], None, None]


class Tests(IsolatedAsyncioTestCase):
    """Run each provided test trace & compare to reference."""

    # shell driver command arguments
    drvr = "./sdriver.pl"
    itsh = "./tsh"
    rtsh = "./tshref"
    args = '"-p"'

    # line match regexes
    # for matching lines in ps
    _psrx = re.compile(r"^ [0-9]{1,10}( pts/[0-9].*)(?:tsh|tshref)?(.*)$")
    _jbrx = re.compile(r"^((?:Job )?\[[0-9]+\]) \([0-9]{1,10}\)(.*)$")
    rgxs = [_psrx, _jbrx]

    @classmethod
    def get_cmd(cls, number: int, impl: str) -> str:
        """Build sdriver command string for given trace number & tiny shell implementation path."""
        return f"{cls.drvr} -t trace{number:02d}.txt -s {impl} -a {cls.args}"

    @classmethod
    async def run_test(cls, number: int, impl: str) -> str:
        """Run test & get output for given trace number & tiny shell implementation."""
        proc = await asyncio.create_subprocess_shell(
            cls.get_cmd(number, impl),
            shell=True, # running in new shell seems to guarantee capture of all output
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, # combine stdout & stderr into one stream
        )

        # get output (all sent to stdout above)
        stdout, _ = await proc.communicate()
        # decode each line to utf-8 from bytestring
        output = stdout.decode()
        return output

    @classmethod
    def run_test_sync(cls, number: int, impl: str) -> str:
        """Run test synchronously."""
        with subprocess.Popen(
            cls.get_cmd(number, impl),
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT
        ) as pipe:
            if pipe.stdout is None:
                raise ValueError("Expected stdout, got None")

            with pipe.stdout as stdout:
                output = "".join( # join output lines
                        [l.decode() # decoded to utf-8
                         for l in stdout.readlines()])

                return output

    @classmethod
    async def exec(cls, number: int) -> tuple[str, str]:
        """Run test & return outputs for given trace number using own shell & ref implementation."""
        act = cls.run_test(number, cls.itsh)
        exp = cls.run_test(number, cls.rtsh)

        return await asyncio.gather(act, exp)

    def assertMultilineEqualExceptPid(self, actual: str, expected: str, msg: str = "") -> None:
        """Assert two multiline strings are equal, except for known locations of PID values."""
        act_lines = actual.split("\n")
        exp_lines = expected.split("\n")

        len_act = len(act_lines)
        len_exp = len(exp_lines)

        # first, ensure same number of lines in output
        self.assertEqual(
                len_act,
                len_exp,
                f"{msg}\n" +
                f"Unequal number of output lines:\n" +
                f"actual (len = {len_act})\n" +
                "--------------------\n" +
                f"{actual}\n" +
                "~~~~~~~~~~~~~~~~~~~~\n" +
                f"expected (len = {len_exp})\n" +
                "--------------------\n" +
                f"{expected}")

        # otherwise, compare line by line
        for idx, act_line in enumerate(act_lines):
            exp_line = exp_lines[idx]

            # check if both lines match any exception regex
            def prd(itm: tuple[Optional[re.Match[str]], Optional[re.Match[str]]]) -> TypeGuard[Match]:
                """Check that a given tuple of possible string iterables is not None for both value."""
                return itm[0] is not None and itm[1] is not None

            match: Optional[Match] = next(                 # get first item
                filter(                                    # such that
                    prd,                                   # item is tuple of Matches
                    ((r.match(act_line), r.match(exp_line)) # when matching actual & expected lines with
                     for r in self.rgxs)),                 # each exception regex
                None)                                      # defaulting to None if no such line exists


            # when at line that starts w/ `[`, compare part before pid &
            # part after pid only as pid is expected to differ
            if (match is not None):
                for act, exp in (g for g in zip(match[0].groups()[1:], match[1].groups()[1:])):
                    self.assertEqual(
                        act,
                        exp,
                        f"{msg}\n" +
                        f"failed to match subgroups in line {idx} of:\n" +
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
                        f"{msg}\n" +
                        f"failed to match in line {idx} of: \n" +
                        "actual\n" +
                        "--------------------\n" +
                        f"{actual}\n" +
                        "~~~~~~~~~~~~~~~~~~~~\n" +
                        "expected\n" +
                        "--------------------\n" +
                        f"{expected}")

    async def test_trace01(self) -> None:
        act, exp = await self.exec(1)
        self.assertMultiLineEqual(act, exp)

    async def test_trace02(self) -> None:
        act, exp = await self.exec(2)
        self.assertMultiLineEqual(act, exp)

    async def test_trace03(self) -> None:
        act, exp = await self.exec(3)
        self.assertMultiLineEqual(act, exp)

    async def test_trace04(self) -> None:
        act, exp = await self.exec(4)
        self.assertMultilineEqualExceptPid(act, exp)

    async def test_trace05(self) -> None:
        act, exp = await self.exec(5)
        self.assertMultilineEqualExceptPid(act, exp)

    async def test_trace06(self) -> None:
        act, exp = await self.exec(6)
        self.assertMultilineEqualExceptPid(act, exp)

    async def test_trace07(self) -> None:
        act, exp = await self.exec(7)
        self.assertMultilineEqualExceptPid(act, exp)

    async def test_trace08(self) -> None:
        act, exp = await self.exec(8)
        self.assertMultilineEqualExceptPid(act, exp)

    async def test_trace09(self) -> None:
        act, exp = await self.exec(9)
        self.assertMultilineEqualExceptPid(act, exp)

    async def test_trace10(self) -> None:
        act, exp = await self.exec(10)
        self.assertMultilineEqualExceptPid(act, exp)

    def test_trace11(self) -> None:
      # ensure these tests are run sequentially to avoid polluting ps output
      act = self.run_test_sync(11, self.itsh)
      exp = self.run_test_sync(11, self.rtsh)
      self.assertMultilineEqualExceptPid(act, exp)

    async def test_trace12(self) -> None:
        # ensure these tests are run sequentially to avoid polluting ps output
        act = self.run_test_sync(12, self.itsh)
        exp = self.run_test_sync(12, self.rtsh)
        self.assertMultilineEqualExceptPid(act, exp)

    async def test_trace13(self) -> None:
        act, exp = await self.exec(13)
        self.assertMultilineEqualExceptPid(act, exp)

    async def test_trace14(self) -> None:
        act, exp = await self.exec(14)
        self.assertMultilineEqualExceptPid(act, exp)

    async def test_trace15(self) -> None:
        act, exp = await self.exec(15)
        self.assertMultilineEqualExceptPid(act, exp)

    async def test_trace16(self) -> None:
        act, exp = await self.exec(16)
        self.assertMultilineEqualExceptPid(act, exp)


if __name__ == "__main__":
    main()
