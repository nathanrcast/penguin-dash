/* --------------------------------------------------------------------
PENGUIN DASH (fork of Extreme Tux Racer)

GLES2 port scaffolding — M0. See glmatrix.h.

Licensed under the GNU General Public License; see COPYING.
---------------------------------------------------------------------*/

#include "glmatrix.h"
#include <cmath>

namespace {
// Local column-major helpers so we don't depend on TVector3d operator set.
struct V3 { double x, y, z; };
V3 sub(const V3& a, const V3& b) { return V3{a.x - b.x, a.y - b.y, a.z - b.z}; }
double dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(const V3& a, const V3& b) {
	return V3{a.y * b.z - a.z * b.y,
	          a.z * b.x - a.x * b.z,
	          a.x * b.y - a.y * b.x};
}
V3 normalize(const V3& a) {
	double len = std::sqrt(dot(a, a));
	if (len < 1e-12) return V3{0, 0, 0};
	return V3{a.x / len, a.y / len, a.z / len};
}
} // namespace

TMatrix<4, 4> MakeOrtho(double l, double r, double b, double t, double n, double f) {
	TMatrix<4, 4> m;
	m.SetIdentity();
	m[0][0] = 2.0 / (r - l);
	m[1][1] = 2.0 / (t - b);
	m[2][2] = -2.0 / (f - n);
	m[3][0] = -(r + l) / (r - l);
	m[3][1] = -(t + b) / (t - b);
	m[3][2] = -(f + n) / (f - n);
	m[3][3] = 1.0;
	return m;
}

TMatrix<4, 4> MakePerspective(double fovYdeg, double aspect, double nearDist, double farDist) {
	TMatrix<4, 4> m;
	m.SetIdentity();
	double fovYrad = fovYdeg * (M_PI / 180.0);
	double fdiv = 1.0 / std::tan(fovYrad / 2.0);
	m[0][0] = fdiv / aspect;
	m[1][1] = fdiv;
	m[2][2] = (farDist + nearDist) / (nearDist - farDist);
	m[2][3] = -1.0;
	m[3][2] = (2.0 * farDist * nearDist) / (nearDist - farDist);
	m[3][3] = 0.0;
	return m;
}

TMatrix<4, 4> MakeLookAt(const TVector3d& eye, const TVector3d& center, const TVector3d& up) {
	V3 e{eye.x, eye.y, eye.z};
	V3 c{center.x, center.y, center.z};
	V3 u{up.x, up.y, up.z};

	V3 fwd = normalize(sub(c, e));
	V3 side = normalize(cross(fwd, u));
	V3 upn = cross(side, fwd);

	TMatrix<4, 4> m;
	m.SetIdentity();
	// column-major m[col][row]
	m[0][0] = side.x; m[1][0] = side.y; m[2][0] = side.z;
	m[0][1] = upn.x;  m[1][1] = upn.y;  m[2][1] = upn.z;
	m[0][2] = -fwd.x; m[1][2] = -fwd.y; m[2][2] = -fwd.z;
	m[3][0] = -dot(side, e);
	m[3][1] = -dot(upn, e);
	m[3][2] = dot(fwd, e);
	m[3][3] = 1.0;
	return m;
}

void MatrixToGL(const TMatrix<4, 4>& m, float out[16]) {
	const double* d = m.data();
	for (int i = 0; i < 16; i++)
		out[i] = static_cast<float>(d[i]);
}

// --------------------------------------------------------------------
TMatrixStack::TMatrixStack() {
	stack.push_back(TMatrix<4, 4>::getIdentity());
}

void TMatrixStack::LoadIdentity() {
	stack.back() = TMatrix<4, 4>::getIdentity();
}

void TMatrixStack::Load(const TMatrix<4, 4>& m) {
	stack.back() = m;
}

void TMatrixStack::MultRight(const TMatrix<4, 4>& m) {
	stack.back() = stack.back() * m;
}

void TMatrixStack::Push() {
	stack.push_back(stack.back());
}

void TMatrixStack::Pop() {
	if (stack.size() > 1)
		stack.pop_back();
}

const TMatrix<4, 4>& TMatrixStack::Top() const {
	return stack.back();
}
