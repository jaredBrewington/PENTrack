/**
 * \file
 * Static, analytical B fields
 *
 * Time-dependent scaling not implemented
 */

#ifndef STATICANALYTICFIELDS_H_
#define STATICANALYTICFIELDS_H_

#include "field.h"

/**
 * Static B field described by:
  * B_x = a1 * exp(- a2* x + a3) + c1
  * B_y = y * a1 * a2 / 2 * exp(- a2* x + a3) + c2
  * B_z = z * a1 * a2 / 2 * exp(- a2* x + a3) + c2
 *
 */
class TExponentialFieldX: public TField{
private:
	double a1, a2, a3, c1, c2;
	double xmin, xmax; ///< min/max x value where the field is "on" [meters]
	double ymin, ymax; ///< maximum y value where the field is "on" [meters]
	double zmin, zmax; ///< maximum z value where the field is "on" [meters]

public:
	/**
	 *
	 * Constructor requires:
	 *
	 * @param _a1 [Tesla]
	 * @param _a2
	 * @param _a3
	 * @param _c1 [Tesla]
	 * @param _c2 [Tesla]
	 * @param _xmax maximum x value for the field to be "on"
	 * @param _xmin minimum x value for the field to be "on"
	 * @param _ymax maximum y value for the field to be "on"
	 * @param _ymin minimum y value for the field to be "on"
	 * @param _zmax maximum z value for the field to be "on"
	 * @param _zmin minimum z value for the field to be "on"
	 *
	 */
	TExponentialFieldX(const double _a1, const double _a2, const double _a3,
		 								const double _c1, const double _c2, const double _xmax, const double _xmin,
										const double _ymax, const double _ymin, const double _zmax, const double _zmin);

	/**
	 * Calculates B field B[3] and the derivatives dBidxj[3][3] for a given point x,y,z
	 *
	 * @param x Cartesian x coordinate
	 * @param y Cartesian y coordinate
	 * @param z Cartesian z coordinate
	 * @param t the time (not used since the field is presumed to be static)
	 * @param B Returns magnetic-field components
	 * @param dBidxj Returns spatial derivatives of magnetic-field components (optional)
	**/
	void BField(const double x, const double y, const double z, const double t, double B[3], double dBidxj[3][3]) const override;

	/**
	 * Adds no electric field.
	 * Required because we are inheriting from abstract class TField which has virtual void EField function.
	 *
	 * @param x Cartesian x coordinate
	 * @param y Cartesian y coordinate
	 * @param z Cartesian z coordinate
	 * @param t Time
	 * @param V Electric potential
	 * @param Ei Electric field components
	 **/
	void EField(const double x, const double y, const double z, const double t, double &V, double Ei[3]) const override {};

private:
	int withinBounds(const double x, const double y, const double z) const;

};

/**
 * Static B field described by:
  * B_z = a1*x + a2
 	*
 */
class TLinearFieldZ: public TField{
private:
	double a1, a2;
	double xmin, xmax; ///< min/max x value where the field is "on" [meters]
	double ymin, ymax; ///< maximum y value where the field is "on" [meters]
	double zmin, zmax; ///< maximum z value where the field is "on" [meters]

public:
	/**
	 *
	 * Constructor requires:
	 *
	 * @param _a1 [Tesla/meter]
	 * @param _a2	[Tesla]
	 * @param _xmax maximum x value for the field to be "on"
	 * @param _xmin minimum x value for the field to be "on"
	 * @param _ymax maximum y value for the field to be "on"
	 * @param _ymin minimum y value for the field to be "on"
	 * @param _zmax maximum z value for the field to be "on"
	 * @param _zmin minimum z value for the field to be "on"
	 *
	 */
	TLinearFieldZ(const double _a1, const double _a2, const double _xmax, const double _xmin,
										const double _ymax, const double _ymin, const double _zmax, const double _zmin);

	/**
	 * Calculates B field B[3] and the derivatives dBidxj[3][3] for a given point x,y,z
	 *
	 * @param x Cartesian x coordinate
	 * @param y Cartesian y coordinate
	 * @param z Cartesian z coordinate
	 * @param t the time (not used since the field is presumed to be static)
	 * @param B Returns magnetic-field components
	 * @param dBidxj Returns spatial derivatives of magnetic-field components (optional)
	**/
	void BField(const double x, const double y, const double z, const double t, double B[3], double dBidxj[3][3]) const override;

	/**
	 * Adds no electric field.
	 * Required because we are inheriting from abstract class TField which has virtual void EField function.
	 *
	 * @param x Cartesian x coordinate
	 * @param y Cartesian y coordinate
	 * @param z Cartesian z coordinate
	 * @param t Time
	 * @param V Electric potential
	 * @param Ei Electric field components
	 **/
	void EField(const double x, const double y, const double z, const double t, double &V, double Ei[3]) const override {};

private:
	int withinBounds(const double x, const double y, const double z) const;

};

#endif /*STATICANALYTICFIELDS_H_*/
