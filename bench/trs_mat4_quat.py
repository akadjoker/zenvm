from __future__ import annotations

from math import cos, sin, sqrt
from time import perf_counter


class Vec3:
    __slots__ = ("x", "y", "z")

    def __init__(self, x: float, y: float, z: float) -> None:
        self.x = x
        self.y = y
        self.z = z

    def __add__(self, other: "Vec3") -> "Vec3":
        return Vec3(self.x + other.x, self.y + other.y, self.z + other.z)

    def __mul__(self, scale: float) -> "Vec3":
        return Vec3(self.x * scale, self.y * scale, self.z * scale)


class Quat:
    __slots__ = ("x", "y", "z", "w")

    def __init__(self, x: float, y: float, z: float, w: float) -> None:
        self.x = x
        self.y = y
        self.z = z
        self.w = w

    def __mul__(self, other: "Quat") -> "Quat":
        return Quat(
            self.w * other.x + self.x * other.w + self.y * other.z - self.z * other.y,
            self.w * other.y - self.x * other.z + self.y * other.w + self.z * other.x,
            self.w * other.z + self.x * other.y - self.y * other.x + self.z * other.w,
            self.w * other.w - self.x * other.x - self.y * other.y - self.z * other.z,
        )


class Mat4:
    __slots__ = (
        "m00", "m01", "m02", "m03",
        "m10", "m11", "m12", "m13",
        "m20", "m21", "m22", "m23",
        "m30", "m31", "m32", "m33",
    )

    def __init__(
        self,
        m00: float, m01: float, m02: float, m03: float,
        m10: float, m11: float, m12: float, m13: float,
        m20: float, m21: float, m22: float, m23: float,
        m30: float, m31: float, m32: float, m33: float,
    ) -> None:
        self.m00 = m00
        self.m01 = m01
        self.m02 = m02
        self.m03 = m03
        self.m10 = m10
        self.m11 = m11
        self.m12 = m12
        self.m13 = m13
        self.m20 = m20
        self.m21 = m21
        self.m22 = m22
        self.m23 = m23
        self.m30 = m30
        self.m31 = m31
        self.m32 = m32
        self.m33 = m33


def quat_axis_angle(ax: float, ay: float, az: float, angle: float) -> Quat:
    length = sqrt(ax * ax + ay * ay + az * az)
    half = angle * 0.5
    s = sin(half) / length
    return Quat(ax * s, ay * s, az * s, cos(half))


def quat_normalize(q: Quat) -> Quat:
    inv = 1.0 / sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w)
    return Quat(q.x * inv, q.y * inv, q.z * inv, q.w * inv)


def mat4_from_trs(p: Vec3, q: Quat, s: Vec3) -> Mat4:
    x2 = q.x + q.x
    y2 = q.y + q.y
    z2 = q.z + q.z
    xx = q.x * x2
    xy = q.x * y2
    xz = q.x * z2
    yy = q.y * y2
    yz = q.y * z2
    zz = q.z * z2
    wx = q.w * x2
    wy = q.w * y2
    wz = q.w * z2

    return Mat4(
        (1.0 - (yy + zz)) * s.x,
        (xy + wz) * s.x,
        (xz - wy) * s.x,
        0.0,
        (xy - wz) * s.y,
        (1.0 - (xx + zz)) * s.y,
        (yz + wx) * s.y,
        0.0,
        (xz + wy) * s.z,
        (yz - wx) * s.z,
        (1.0 - (xx + yy)) * s.z,
        0.0,
        p.x,
        p.y,
        p.z,
        1.0,
    )


def bench_trs(n: int) -> float:
    pos = Vec3(1.0, 2.0, 3.0)
    scale = Vec3(1.25, 0.75, 1.5)
    move = Vec3(0.001, -0.0003, 0.0002)
    rot = Quat(0.0, 0.0, 0.0, 1.0)
    step = quat_axis_angle(0.3, 0.7, 0.2, 0.002)
    acc = 0.0

    for i in range(n):
        rot = quat_normalize(rot * step)
        pos = pos + move
        m = mat4_from_trs(pos, rot, scale)
        v = i * 0.0001
        acc = acc + m.m00 * v + m.m11 * 0.25 + m.m22 * 0.125 + m.m30 + m.m31 + m.m32

    return acc + rot.x + rot.y + rot.z + rot.w + pos.x + pos.y + pos.z


N = 100_000
t0 = perf_counter()
result = bench_trs(N)
t1 = perf_counter()
print("python_trs_mat4_quat")
print(f"n={N}")
print(f"seconds={t1 - t0}")
print(f"result={result}")
