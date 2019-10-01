/****************************************************************************
 * Core Library Version 1.7, August 2004                                     
 * Copyright (c) 1995-2004 Exact Computation Project                         
 * All rights reserved.                                                      
 *                                                                           
 * This file is part of CGAL (www.cgal.org).                
 * You can redistribute it and/or modify it under the terms of the GNU       
 * Lesser General Public License as published by the Free Software Foundation,      
 * either version 3 of the License, or (at your option) any later version.   
 *                                                                           
 * Licensees holding a valid commercial license may use this file in         
 * accordance with the commercial license agreement provided with the        
 * software.                                                                 
 *                                                                           
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE   
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 *                                                                           
 *                                                                           
 * $URL: https://github.com/CGAL/cgal/blob/releases/CGAL-4.14.1/CGAL_Core/include/CGAL/CORE/linearAlgebra.h $
 * $Id: linearAlgebra.h 8cdfad0 %aI Sébastien Loriot
 * SPDX-License-Identifier: LGPL-3.0+
 ***************************************************************************/
/******************************************************************
 * Core Library Version 1.7, August 2004
 * Copyright (c) 1995-2002 Exact Computation Project
 * 
 * File: LinearAlgebra.h
 * Synopsis:
 *      Linear Algebra Extension of Core Library introducing
 *              class Vector
 *              class Matrix
 *
 * Written by
 *       Shubin Zhao (shubinz@cs.nyu.edu) (2001)
 *
 * WWW URL: http://cs.nyu.edu/exact/
 * Email: exact@cs.nyu.edu
 *
 * $Id: linearAlgebra.h 8cdfad0 %aI Sébastien Loriot
 *****************************************************************/

#ifndef CORE_LINEAR_ALGEBRA_H
#define CORE_LINEAR_ALGEBRA_H

#ifndef CORE_LEVEL
#  define CORE_LEVEL 3
#endif

#include <cstdarg>
#include <CGAL/CORE/CORE.h>

class Vector;
class Matrix;

////////////////////////////////////////////////////////////////////////
//  Class Vector
//     Generic vectors
//     Operations implemented:  addition, subtraction, dot product
////////////////////////////////////////////////////////////////////////

class Vector {
private:
   int     dim;
   double* _rep;
public:
   class RangeException { };
   class ArithmeticException { };

   explicit Vector(int);
   Vector();
   Vector(double, double);
   Vector(double, double, double);
   Vector(const Vector&);
   Vector(int, double *);
   ~Vector();

   const Vector& operator=(const Vector&);

   bool operator==(const Vector&);
   bool operator!=(const Vector&);
   const Vector& operator+=(const Vector&);
   const Vector& operator-=(const Vector&);
   const Vector& operator*=(double);

   const double& operator[](int) const;
   double& operator[](int);

   double norm() const;
   double maxnorm() const;
   double infnorm() const;
   double dimension() const {return dim;}
   bool isZero() const;
   Vector cross(const Vector &v) const; 
   static Vector crossProduct(int, ...);

   friend Vector operator+(const Vector&, const Vector&);
   friend Vector operator-(const Vector&, const Vector&);
   friend Vector operator-(const Vector&);
   friend Vector operator*(const Vector&, double);
   friend Vector operator*(double, const Vector&);
   friend Vector operator*(const Matrix&, const Vector&);
   friend Vector operator*(const Vector&, const Matrix&);
   friend double dotProduct(const Vector&, const Vector&);

   friend std::istream& operator>>(std::istream&, Vector&);
   friend std::ostream& operator<<(std::ostream&, const Vector&);
};

////////////////////////////////////////////////////////////////////////
//  Class Matrix
//     Generic matrices
//     Operations implemented:  addition, subtraction, multiplication
////////////////////////////////////////////////////////////////////////

class Matrix {
private:
   int dim1, dim2;
   double* _rep;

public:
   class RangeException { };
   class ArithmeticException { };

   explicit Matrix(int);
   Matrix(int, int);
   Matrix(int, int, double *);
   Matrix(double, double,
          double, double);
   Matrix(double, double, double,
          double, double, double,
          double, double, double);
   Matrix(const Matrix&);
   ~Matrix();

   Matrix& operator=(const Matrix&);

   bool operator==(const Matrix&);
   bool operator!=(const Matrix&);

   const Matrix& operator+=(const Matrix&);
   const Matrix& operator-=(const Matrix&);
   const Matrix& operator*=(double);

   const double& operator()(int, int) const;
   double& operator()(int, int);

   // added by chen li
   //   const Vector& row(int i) const;
   //   const Vector& col(int i) const;
   Matrix matrixAlgebraRemainder(int, int) const;
   double valueAlgebraRemainder(int, int) const;
   const Matrix& transpose();

   double determinant() const;

   int dimension_1() const { return dim1; }
   int dimension_2() const { return dim2; }

   friend Matrix operator+(const Matrix&, const Matrix&);
   friend Matrix operator-(const Matrix&, const Matrix&);
   friend Matrix operator*(const Matrix&, double);
   friend Matrix operator*(double, const Matrix&);
   friend Vector operator*(const Vector&, const Matrix&);
   friend Vector operator*(const Matrix&, const Vector&);
   friend Matrix operator*(const Matrix&, const Matrix&);
   friend Matrix transpose(const Matrix&);

   friend double det(const double a, const double b,
                const double c, const double d);
   friend double det(const Vector u, const Vector & v);  // u,v are 2d vectors
   
   friend std::istream& operator>>(std::istream&, Matrix&);
   friend std::ostream& operator<<(std::ostream&, const Matrix&);

}; //Matrix

#endif
