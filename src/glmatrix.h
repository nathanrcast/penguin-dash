/* --------------------------------------------------------------------
PENGUIN DASH (fork of Extreme Tux Racer)

GLES2 port scaffolding — M0. Explicit matrix builders + a software matrix
stack to replace the fixed-function GL matrix stack. Matrices are column-major,
matching TMatrix storage (translation at data()[12..14]) and the layout expected
by glUniformMatrix4fv(..., transpose = GL_FALSE, ...).

Licensed under the GNU General Public License; see COPYING.
---------------------------------------------------------------------*/

#ifndef GLMATRIX_H
#define GLMATRIX_H

#include "matrices.h"
#include "vectors.h"
#include <vector>

// Projection / view builders (return column-major TMatrix<4,4>).
TMatrix<4, 4> MakeOrtho(double l, double r, double b, double t, double n, double f);
TMatrix<4, 4> MakePerspective(double fovYdeg, double aspect, double nearDist, double farDist);
TMatrix<4, 4> MakeLookAt(const TVector3d& eye, const TVector3d& center, const TVector3d& up);

// Column-major float[16] for glUniformMatrix4fv(..., GL_FALSE, ...).
void MatrixToGL(const TMatrix<4, 4>& m, float out[16]);

// Minimal replacement for the fixed-function matrix stack (used from M1 on).
class TMatrixStack {
	std::vector<TMatrix<4, 4> > stack;
public:
	TMatrixStack();
	void LoadIdentity();
	void Load(const TMatrix<4, 4>& m);
	void MultRight(const TMatrix<4, 4>& m); // top = top * m
	void Push();
	void Pop();
	const TMatrix<4, 4>& Top() const;
};

#endif
