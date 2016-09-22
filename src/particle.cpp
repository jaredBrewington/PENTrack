/**
 * \file
 * Particle base class definition.
 */

#include <cmath>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <sys/time.h>

#include <boost/numeric/odeint.hpp>

#include "particle.h"

using namespace std;

double TParticle::Hstart(){
	return Ekin(&ystart[3]) + Epot(tstart, ystart, field, geom->GetSolid(tstart, &ystart[0]));
}


double TParticle::Hend(){
	return Ekin(&yend[3]) + Epot(tend, yend, field, geom->GetSolid(tend, &yend[0]));
}


double TParticle::Estart(){
	return Ekin(&ystart[3]);
}


double TParticle::Eend(){
	return Ekin(&yend[3]);
}

TParticle::TParticle(const char *aname, const  double qq, const long double mm, const long double mumu, const long double agamma, int number,
		double t, double x, double y, double z, double E, double phi, double theta, double polarisation, TMCGenerator &amc, TGeometry &geometry, TFieldManager *afield)
		: name(aname), q(qq), m(mm), mu(mumu), gamma(agamma), particlenumber(number), ID(ID_UNKNOWN),
		  tstart(t), tend(t), Hmax(0), lend(0), Nhit(0), Nspinflip(0), noflipprob(1), Nstep(0),
		  wL(0), delwL(0), geom(&geometry), mc(&amc), field(afield){
	value_type Eoverm = E/m/c_0/c_0; // gamma - 1
	value_type beta = sqrt(1-(1/((Eoverm + 1)*(Eoverm + 1)))); // beta = sqrt(1 - 1/gamma^2)
	value_type vstart;
//			cout << "(E/m)^3.5 = " << pow(Eoverm, 3.5L) << "; err_beta = " << numeric_limits<value_tyoe>::epsilon()/(1 - beta) << '\n';
	if (pow(Eoverm, 3.5L) < numeric_limits<value_type>::epsilon()/(1 - beta)) // if error in series O((E/m)^3.5) is smaller than rounding error in beta
		vstart = c_0*(sqrt(2*Eoverm) - 3*pow(Eoverm, 1.5L)/2/sqrt(2) + 23*pow(Eoverm, 2.5L)/16/sqrt(2)); // use series expansion
	else
		vstart = c_0 * beta;

	if (polarisation < -1 || polarisation > 1){
		std::cout << "Polarisation is " << polarisation << "! Has to be between -1 and 1!\n";
		exit(-1);
	}

	ystart.resize(STATE_VARIABLES, 0);
	ystart[0] = x; // position
	ystart[1] = y;
	ystart[2] = z;
	ystart[3] = vstart*cos(phi)*sin(theta); // velocity
	ystart[4] = vstart*sin(phi)*sin(theta);
	ystart[5] = vstart*cos(theta);
	ystart[6] = 0; // proper time
	ystart[7] = amc.DicePolarisation(polarisation); // choose initial polarisation randomly, weighted by spin projection onto magnetic field
	yend = ystart;

	spinstart.resize(3, 0);
	if (field){
		double B[4][4];
		field->BField(x, y, z, t, B);
		if (B[3][0] > 0){
			double spinaz = mc->UniformDist(0, 2*pi); // random azimuth angle of spin
			spinstart[0] = sqrt(1 - polarisation*polarisation)*sin(spinaz); // set initial spin vector
			spinstart[1] = sqrt(1 - polarisation*polarisation)*cos(spinaz);
			spinstart[2] = polarisation;
			double B0[3] = {B[0][0], B[1][0], B[2][0]};
			RotateVector(&spinstart[0], B0); // rotate initial spin vector such that its z-component is parallel to magnetic field
		}
	}
	spinend = spinstart;

	Hmax = Hstart();
	maxtraj = mc->MaxTrajLength(name); // max. trajectory length
	tau = mc->LifeTime(name); // lifetime either exponentially distributed or fixed
	geom->GetSolids(t, &ystart[0], currentsolids); // detect solids that surround the particle
	solidend = solidstart = GetCurrentsolid(); // set to solid with highest priority
}


TParticle::~TParticle(){
	for (vector<TParticle*>::reverse_iterator i = secondaries.rbegin(); i != secondaries.rend(); i++)
		delete *i;
}


void TParticle::operator()(state_type y, state_type &dydx, value_type x){
	derivs(x,y,dydx);
}


void TParticle::Integrate(double tmax, map<string, string> &conf){
	if (currentsolids.empty())
		geom->GetSolids(tend, &yend[0], currentsolids);

	int perc = 0;
	cout << "Particle no.: " << particlenumber << " particle type: " << name << '\n';
	cout << "x: " << yend[0] << "m y: " << yend[1] << "m z: " << yend[2]
		 << "m E: " << Eend() << "eV t: " << tend << "s tau: " << tau << "s lmax: " << maxtraj << "m\n";

	// set initial values for integrator
	value_type x = tend, x1, x2;
	state_type y = yend; 
	state_type y1(STATE_VARIABLES), y2(STATE_VARIABLES);

	value_type h = 0.001/sqrt(y[3]*y[3] + y[4]*y[4] + y[5]*y[5]); // first guess for stepsize

	bool resetintegration = true;

	float nextsnapshot = -1;
	bool snapshotlog = false;
	istringstream(conf["snapshotlog"]) >> snapshotlog;
	istringstream snapshots(conf["snapshots"]);
	if (snapshotlog){
		do{
			snapshots >> nextsnapshot;
		}while (snapshots.good() && nextsnapshot < x); // find first snapshot time
	}

	bool tracklog = false;
	istringstream(conf["tracklog"]) >> tracklog;
	if (tracklog)
		PrintTrack(tend, yend, solidend);
	double trackloginterval = 1e-3;
	istringstream(conf["trackloginterval"]) >> trackloginterval;
	value_type lastsave = x;

	bool hitlog = false;
	istringstream(conf["hitlog"]) >> hitlog;

	bool simulEFieldSpinInteg = false; 
	istringstream(conf["simulEFieldSpinInteg"]) >> simulEFieldSpinInteg;

	bool flipspin;
	istringstream(conf["flipspin"]) >> flipspin;
	
	int spinlog;
	double SpinBmax, spinloginterval;
	vector<double> SpinTimes;
	std::istringstream(conf["BFmaxB"]) >> SpinBmax;
	std::istringstream SpinTimess(conf["BFtimes"]);
	do{
		double t;
		SpinTimess >> t;
		if (SpinTimess.good())
			SpinTimes.push_back(t);
	}while(SpinTimess.good());
	std::istringstream(conf["spinlog"]) >> spinlog;
	std::istringstream(conf["spinloginterval"]) >> spinloginterval;
	state_type spin = spinend;

	stepper = boost::numeric::odeint::make_dense_output(1e-9, 1e-9, stepper_type());

	while (ID == ID_UNKNOWN){ // integrate as long as nothing happened to particle
		if (resetintegration){
			stepper.initialize(y, x, h);
		}
		x1 = x; // save point before next step
		y1 = y;

		try{
			stepper.do_step(boost::ref(*this));
			x = stepper.current_time();
			y = stepper.current_state();
			h = stepper.current_time_step();
			Nstep++;
		}
		catch(...){ // catch Exceptions thrown by numerical recipes routines
			StopIntegration(ID_ODEINT_ERROR, x, y, GetCurrentsolid());
		}

		if (y[6] > tau){ // if proper time is larger than lifetime
			x = x1 + (x - x1)*(tau - y1[6])/(y[6] - y1[6]); // interpolate decay time in lab frame
			stepper.calc_state(x, y);
		}
		if (stepper.current_time() > tmax){	//If stepsize overshot max simulation time
			x = tmax;
			stepper.calc_state(tmax, y);
		}

		while (x1 < x){ // split integration step in pieces (x1,y1->x2,y2) with spatial length SAMPLE_DIST, go through all pieces
			value_type v1 = sqrt(y1[3]*y1[3] + y1[4]*y1[4] + y1[5]*y1[5]);
			x2 = x1 + MAX_SAMPLE_DIST/v1; // time length = spatial length/velocity
			if (x2 >= x){
				x2 = x;
				y2 = y;
			}
			else{
				stepper.calc_state(x2, y2);
			}

			resetintegration = CheckHit(x1, y1, x2, y2, hitlog); // check if particle hit a material boundary or was absorbed between y1 and y2
			if (resetintegration){
				x = x2; // if particle path was changed: reset integration end point
				y = y2;
			}

			IntegrateSpin(spin, x1, y1, x2, y2, SpinTimes, SpinBmax, flipspin, spinlog, spinloginterval);
			spinend = spin;

			lend += sqrt(pow(y2[0] - y1[0], 2) + pow(y2[1] - y1[1], 2) + pow(y2[2] - y1[2], 2));
			Hmax = max(Ekin(&y2[3]) + Epot(x, y2, field, GetCurrentsolid()), Hmax);

			// take snapshots at certain times
			if (snapshotlog && snapshots.good()){
				if (x1 <= nextsnapshot && x2 > nextsnapshot){
					state_type ysnap(STATE_VARIABLES);
					stepper.calc_state(nextsnapshot, ysnap);
					cout << "\n Snapshot at " << nextsnapshot << " s \n";

					PrintSnapshot(nextsnapshot, ysnap, GetCurrentsolid());
					snapshots >> nextsnapshot;
				}
			}

			if (tracklog && x2 - lastsave > trackloginterval/v1){
				PrintTrack(x2, y2, GetCurrentsolid());
				lastsave = x2;
			}

			x1 = x2;
			y1 = y2;
		}

		PrintPercent(max(y[6]/tau, max((x - tstart)/(tmax - tstart), lend/maxtraj)), perc);
		
		if (ID == ID_UNKNOWN && y[6] >= tau) // proper time >= tau?
			StopIntegration(ID_DECAYED, x, y, GetCurrentsolid());
		else if (ID == ID_UNKNOWN && (x >= tmax || lend >= maxtraj)) // time > tmax or trajectory length > max length?
			StopIntegration(ID_NOT_FINISH, x, y, GetCurrentsolid());
	}

	Print(tend, yend, solidend);

	if (ID == ID_DECAYED){ // if particle reached its lifetime call TParticle::Decay
		cout << "Decayed!\n";
		Decay();
	}

	cout << "x: " << yend[0];
	cout << " y: " << yend[1];
	cout << " z: " << yend[2];
	cout << " E: " << Eend();
	cout << " Code: " << ID;
	cout << " t: " << tend;
	cout << " l: " << lend;
	cout << " hits: " << Nhit;
	cout << " spinflips: " << Nspinflip << '\n';
	cout << "Computation took " << Nstep << " steps\n";
	cout << "Done!!\n\n";
//			cout.flush();
}


void TParticle::IntegrateSpin(state_type &spin, value_type x1, state_type y1, value_type x2, state_type &y2, std::vector<double> &times, double Bmax, bool flipspin, bool spinlog, double spinloginterval){
	if (gamma == 0 || x1 == x2 || !field)
		return;

	double B1[4][4], B2[4][4], polarisation;
	field->BField(y1[0], y1[1], y1[2], x1, B1);
	field->BField(y2[0], y2[1], y2[2], x2, B2);

	if (B2[3][0] == 0){ // if there's no magnetic field or particle is leaving magnetic field, do nothing
		std::cout << "Magnetic field is zero\n";
		std::fill(spin.begin(), spin.end(), 0); // set all spin components to zero
		return;
	}
	else if (B1[3][0] == 0 && B2[3][0] > 0){ // if particle enters magnetic field
		std::cout << "Entering magnetic field\n";
		if (!flipspin) // if polarization should stay constant
			polarisation = y1[7]; // take polarization from state vector
		else{
			if (mc->UniformDist(0,1) < 0.5) // if spin flips are allowed, choose random polarization
				polarisation = 1;
			else
				polarisation = -1;
		}
		spin[0] = polarisation*B2[0][0]/B2[3][0]; // set spin (anti-)parallel to field
		spin[1] = polarisation*B2[1][0]/B2[3][0];
		spin[2] = polarisation*B2[2][0]/B2[3][0];
		return;
	}
	else{ // if particle already is in magnetic field, calculate spin-projection on magnetic field
		polarisation = (spin[0]*B1[0][0] + spin[1]*B1[1][0] + spin[2]*B1[2][0])/B1[3][0]/sqrt(spin[0]*spin[0] + spin[1]*spin[1] + spin[2]*spin[2]);
	}

	bool integrate1 = false, integrate2 = false;;
	for (unsigned int i = 0; i < times.size(); i += 2){
		integrate1 |= (x1 >= times[i] && x1 < times[i+1]);
		integrate2 |= (x2 >= times[i] && x2 < times[i+1]);
	}

	if ((integrate1 || integrate2) && (B1[3][0] < Bmax || B2[3][0] < Bmax)){ // do spin integration only, if time is in specified range and field is smaller than Bmax
		std::cout << x2-x1 << "s " << 1 - polarisation << " ";

		alglib::real_1d_array ts; // set up values for interpolation of precession axis
		vector<alglib::real_1d_array> omega(3);
		vector<alglib::spline1dinterpolant> omega_int(3);
		const int int_points = 10;
		ts.setlength(int_points + 1);
		for (int i = 0; i < 3; i++)
			omega[i].setlength(int_points + 1);

		for (int i = 0; i <= int_points; i++){ // calculate precession axis at several points along trajectory step
			double t = x1 + i*(x2 - x1)/int_points;
			double B[4][4], V, E[3];
			state_type y(STATE_VARIABLES), dydt(STATE_VARIABLES);
			stepper.calc_state(t, y);
			derivs(t, y, dydt);
			field->BField(y[0], y[1], y[2], t, B);
			field->EField(y[0], y[1], y[2], t, V, E);
			ts[i] = t;
			double v2 = y[3]*y[3] + y[4]*y[4] + y[5]*y[5];
			double gamma_rel = 1./sqrt(1 - v2/c_0/c_0);
			double Bparallel[3];
			for (int j = 0; j < 3; j++)
				Bparallel[j] = (B[0][0]*y[3] + B[1][0]*y[4] + B[2][0]*y[5])*y[j]/v2; // magnetic-field component parallel to velocity

			// spin precession axis due to relativistically distorted magnetic field, omega_B = -gyro/gamma * ( (1 - gamma)*(v.B)*v/v^2 + gamma*B - gamma*(v x E)/c )
			double OmegaB[3];
			OmegaB[0] = -gamma/gamma_rel * ((1 - gamma_rel)*Bparallel[0] + gamma_rel*B[0][0] - gamma_rel*(y[4]*E[2] - y[5]*E[2])/c_0);
			OmegaB[1] = -gamma/gamma_rel * ((1 - gamma_rel)*Bparallel[1] + gamma_rel*B[1][0] - gamma_rel*(y[5]*E[0] - y[3]*E[0])/c_0);
			OmegaB[2] = -gamma/gamma_rel * ((1 - gamma_rel)*Bparallel[2] + gamma_rel*B[2][0] - gamma_rel*(y[3]*E[1] - y[4]*E[1])/c_0);

			// Thomas precession in lab frame, omega_T = gamma^2/(gamma + 1)*(dv/dt x v)
			double OmegaT[3];
			OmegaT[0] = gamma_rel*gamma_rel/(gamma_rel + 1)*(dydt[4]*y[5] - dydt[5]*y[4])/c_0/c_0;
			OmegaT[1] = gamma_rel*gamma_rel/(gamma_rel + 1)*(dydt[5]*y[3] - dydt[3]*y[5])/c_0/c_0;
			OmegaT[2] = gamma_rel*gamma_rel/(gamma_rel + 1)*(dydt[3]*y[4] - dydt[4]*y[3])/c_0/c_0;

			// total precession axis is sum of magnetic precession axis and Thomas-precession axis
			omega[0][i] = OmegaB[0] + OmegaT[0];
			omega[1][i] = OmegaB[1] + OmegaT[1];
			omega[2][i] = OmegaB[2] + OmegaT[2];
		}

		std::cout << (x2 - x1)*sqrt(omega[0][0]*omega[0][0] + omega[1][0]*omega[1][0] + omega[2][0]*omega[2][0])/2/pi << "r ";
		for (int i = 0; i < 3; i++)
			alglib::spline1dbuildcubic(ts, omega[i], omega_int[i]); // interpolate all three components of precession axis

		dense_stepper_type spinstepper = boost::numeric::odeint::make_dense_output(static_cast<value_type>(1e-12), static_cast<value_type>(1e-12), stepper_type());
		spinstepper.initialize(spin, x1, std::abs(pi/gamma/B1[3][0])); // initialize integrator with step size = half rotation
		unsigned int steps = 0;
		while (true){
			// take an integration step, SpinDerivs contains right-hand side of equation of motion
			spinstepper.do_step(boost::bind(&TParticle::SpinDerivs, this, _1, _2, _3, omega_int));
			steps++;
			if (spinstepper.current_time() >= x2){ // if stepper overshot, calculate end point and stop
				spinstepper.calc_state(x2, spin);
				break;
			}

		}

		// calculate new spin projection
		polarisation = (spin[0]*B2[0][0] + spin[1]*B2[1][0] + spin[2]*B2[2][0])/B2[3][0]/sqrt(spin[0]*spin[0] + spin[1]*spin[1] + spin[2]*spin[2]);
		std::cout << " " << 1 - polarisation << " (" << steps << ")\n";
	}
	else if ((B1[3][0] < Bmax || B2[3][0] < Bmax)){ // if time outside selected ranges, parallel-transport spin along magnetic field
		if (polarisation*polarisation >= 1){ // catch rounding errors
			spin[0] = 0;
			spin[1] = 0;
		}
		else{
			double spinaz = mc->UniformDist(0, 2*pi); // random azimuth
			spin[0] = sqrt(1 - polarisation*polarisation)*sin(spinaz);
			spin[1] = sqrt(1 - polarisation*polarisation)*cos(spinaz);
		}
		spin[2] = polarisation;
		double B0[3] = {B2[0][0], B2[1][0], B2[2][0]};
		RotateVector(&spin[0], B0); // rotate spin vector onto magnetic field vector
	}


	if (B2[3][0] > Bmax){ // if magnetic field grows above Bmax, collapse spin state to one of the two polarisation states
		if (flipspin)
			y2[7] = mc->DicePolarisation(polarisation); // set new polarisation weighted by spin-projection on magnetic field
		spin[0] = B2[0][0]*y2[7]/B2[3][0];
		spin[1] = B2[1][0]*y2[7]/B2[3][0];
		spin[2] = B2[2][0]*y2[7]/B2[3][0];
	}
}


void TParticle::SpinDerivs(state_type y, state_type &dydx, value_type x, std::vector<alglib::spline1dinterpolant> &omega){
	double omegat[3];
	for (int i = 0; i < 3; i++)
		omegat[i] = alglib::spline1dcalc(omega[i], x);
	dydx[0] = omegat[1]*y[2] - omegat[2]*y[1];
	dydx[1] = omegat[2]*y[0] - omegat[0]*y[2];
	dydx[2] = omegat[0]*y[1] - omegat[1]*y[0];
}


solid TParticle::GetCurrentsolid(){
	map<solid, bool>::iterator it = currentsolids.begin();
	while (it->second) // skip over ignored solids
		it++;
	return it->first; // return first non-ignored solid in list
}


void TParticle::derivs(value_type x, state_type y, state_type &dydx){
	dydx[0] = y[3]; // time derivatives of position = velocity
	dydx[1] = y[4];
	dydx[2] = y[5];

	value_type F[3] = {0,0,0}; // Force in lab frame
	F[2] += -gravconst*m*ele_e; // add gravitation to force
	if (field){
		double B[4][4], E[3], V; // magnetic/electric field and electric potential in lab frame
		if (q != 0 || (mu != 0 && y[7] != 0))
			field->BField(y[0],y[1],y[2], x, B); // if particle has charge or magnetic moment, calculate magnetic field
		if (q != 0)
			field->EField(y[0],y[1],y[2], x, V, E); // if particle has charge caculate electric field+potential
		if (q != 0){
			F[0] += q*(E[0] + y[4]*B[2][0] - y[5]*B[1][0]); // add Lorentz-force
			F[1] += q*(E[1] + y[5]*B[0][0] - y[3]*B[2][0]);
			F[2] += q*(E[2] + y[3]*B[1][0] - y[4]*B[0][0]);
		}
		if (mu != 0 && y[7] != 0){
			F[0] += y[7]*mu*B[3][1]; // add force on magnetic dipole moment
			F[1] += y[7]*mu*B[3][2];
			F[2] += y[7]*mu*B[3][3];
		}
	}
	value_type inversegamma = sqrt(1 - (y[3]*y[3] + y[4]*y[4] + y[5]*y[5])/(c_0*c_0)); // relativstic factor 1/gamma
	dydx[3] = inversegamma/m/ele_e*(F[0] - (y[3]*y[3]*F[0] + y[3]*y[4]*F[1] + y[3]*y[5]*F[2])/c_0/c_0); // general relativstic equation of motion
	dydx[4] = inversegamma/m/ele_e*(F[1] - (y[4]*y[3]*F[0] + y[4]*y[4]*F[1] + y[4]*y[5]*F[2])/c_0/c_0); // dv/dt = 1/gamma/m*(F - v * v^T * F / c^2)
	dydx[5] = inversegamma/m/ele_e*(F[2] - (y[5]*y[3]*F[0] + y[5]*y[4]*F[1] + y[5]*y[5]*F[2])/c_0/c_0);

	dydx[6] = inversegamma; // derivative of proper time is 1/gamma
	dydx[7] = 0; // polarisaton does not change
}


bool TParticle::CheckHitError(solid *hitsolid, double distnormal){
	if (distnormal < 0){ // particle is entering solid
		if (currentsolids.find(*hitsolid) != currentsolids.end()){ // if solid already is in currentsolids list something went wrong
			cout << "Particle entering " << hitsolid->name.c_str() << " which it did enter before! Stopping it! Did you define overlapping solids with equal priorities?\n";
			return true;
		}
	}
	else if (distnormal > 0){ // particle is leaving solid
		if (currentsolids.find(*hitsolid) == currentsolids.end()){ // if solid is not in currentsolids list something went wrong
			cout << "Particle inside '" << hitsolid->name << "' which it did not enter before! Stopping it!\n";
			return true;
		}
	}
	else{
		cout << "Particle is crossing material boundary with parallel track! Stopping it!\n";
		return true;
	}
	return false;
}


bool TParticle::CheckHit(value_type x1, state_type y1, value_type &x2, state_type &y2, bool hitlog, int iteration){
	solid currentsolid = GetCurrentsolid();
	if (!geom->CheckSegment(&y1[0], &y2[0])){ // check if start point is inside bounding box of the simulation geometry
		printf("\nParticle has hit outer boundaries: Stopping it! t=%g x=%g y=%g z=%g\n",x2,y2[0],y2[1],y2[2]);
		StopIntegration(ID_HIT_BOUNDARIES, x2, y2, currentsolid);
		return true;
	}

	map<TCollision, bool> colls;
	bool collfound = false;
	try{
		collfound = geom->GetCollisions(x1, &y1[0], x2, &y2[0], colls);
	}
	catch(...){
		StopIntegration(ID_CGAL_ERROR, x2, y2, currentsolid);
		return true;
	}

	if (collfound){	// if there is a collision with a wall
		TCollision coll = colls.begin()->first;
		if ((abs(coll.s*coll.distnormal) < REFLECT_TOLERANCE
			&& (1 - coll.s)*abs(coll.distnormal) < REFLECT_TOLERANCE) // if first collision is closer to y1 and y2 than REFLECT_TOLERANCE
			|| iteration > 99) // or if iteration counter reached a certain maximum value
		{
			bool trajectoryaltered = false, traversed = true;

			map<solid, bool> newsolids = currentsolids;
			for (map<TCollision, bool>::iterator it = colls.begin(); it != colls.end(); it++){ // go through list of collisions
				solid sld = geom->solids[it->first.sldindex];
				if (CheckHitError(&sld, it->first.distnormal)){ // check all hits for errors
					x2 = x1;
					y2 = y1;
					StopIntegration(ID_GEOMETRY_ERROR, x2, y2, currentsolid); // stop trajectory integration if error found
					return true;
				}

				if (it->first.distnormal < 0){ // if particle is entering hit surface
//					cout << "-->" << sld.name << ' ';
					newsolids[sld] = it->second; // add new solid to list
				}
				else{ // if particle is leaving hit surface
//					cout << sld.name << "--> ";
					newsolids.erase(sld); // remove solid from list of new solids
				}

				if (sld.ID > geom->solids[coll.sldindex].ID)
					coll = it->first; // use geometry from collision with highest priority
}
//			cout << '\n';

			solid leaving = currentsolid; // particle can only leave highest-priority solid
			solid entering = geom->defaultsolid;
			for (map<solid, bool>::iterator it = newsolids.begin(); it != newsolids.end(); it++){ // go through list of new solids
				if (!it->second && it->first.ID > entering.ID){ // highest-priority solid in newsolids list will be entered in collisions
					entering = it->first;
				}
			}
//			cout << "Leaving " << leaving.name << ", entering " << entering.name << ", currently " << currentsolid.name << '\n';
			if (leaving.ID != entering.ID){ // if the particle actually traversed a material interface
				OnHit(x1, y1, x2, y2, coll.normal, &leaving, &entering, trajectoryaltered, traversed); // do particle specific things
				if (hitlog)
					PrintHit(x1, y1, y2, coll.normal, &leaving, &entering); // print collision to file if requested
				Nhit++;
			}

			if (traversed){
				currentsolids = newsolids; // if surface was traversed (even if it was  physically ignored) replace current solids with list of new solids
			}

			if (trajectoryaltered)
				return true;
			else if (OnStep(x1, y1, x2, y2, GetCurrentsolid())){ // check for absorption
				printf("Absorption!\n");
				return true;
			}

		}
		else{
			// else cut integration step right before and after first collision point
			// and call ReflectOrAbsorb again for each smaller step (quite similar to bisection algorithm)
			value_type xnew, xbisect1 = x1, xbisect2 = x1;
			state_type ybisect1(STATE_VARIABLES);
			state_type ybisect2(STATE_VARIABLES);
			ybisect1 = ybisect2 = y1;

			xnew = x1 + (x2 - x1)*(coll.s - 0.01*iteration); // cut integration right before collision point
			if (xnew > x1 && xnew < x2){ // check that new line segment is in correct time interval
				xbisect1 = xbisect2 = xnew;
				stepper.calc_state(xbisect1, ybisect1);
				ybisect2 = ybisect1;
				if (CheckHit(x1, y1, xbisect1, ybisect1, hitlog, iteration+1)){ // recursive call for step before coll. point
					x2 = xbisect1;
					y2 = ybisect1;
					return true;
				}
			}

			xnew = x1 + (x2 - x1)*(coll.s + 0.01*iteration); // cut integration right after collision point
			if (xnew > xbisect1 && xnew < x2){ // check that new line segment does not overlap with previous one
				xbisect2 = xnew;
				stepper.calc_state(xbisect2, ybisect2);
				if (CheckHit(xbisect1, ybisect1, xbisect2, ybisect2, hitlog, iteration+1)){ // recursive call for step over coll. point
					x2 = xbisect2;
					y2 = ybisect2;
					return true;
				}
			}

			if (CheckHit(xbisect2, ybisect2, x2, y2, hitlog, iteration+1)) // recursive call for step after coll. point
				return true;
		}
	}
	else if (OnStep(x1, y1, x2, y2, GetCurrentsolid())){ // if there was no collision: just check for absorption in solid with highest priority
		return true;
	}
	return false;
}


void TParticle::StopIntegration(int aID, value_type x, state_type y, solid sld){
	ID = aID;
	tend = x;
	yend = y;
	solidend = sld;
}


void TParticle::Print(std::ofstream &file, value_type x, state_type y, solid sld, std::string filesuffix){
	if (!file.is_open()){
		ostringstream filename;
		filename << outpath << '/' << setw(12) << setfill('0') << jobnumber << name << filesuffix;
		cout << "Creating " << filename.str() << '\n';
		file.open(filename.str().c_str());
		if (!file.is_open()){
			cout << "Could not create" << filename.str() << '\n';
			exit(-1);
		}
		file <<	"jobnumber particle "
					"tstart xstart ystart zstart "
					"vxstart vystart vzstart polstart "
					"Hstart Estart Bstart Ustart solidstart "
					"tend xend yend zend "
					"vxend vyend vzend polend "
					"Hend Eend Bend Uend solidend "
					"stopID Nspinflip spinflipprob "
					"Nhit Nstep trajlength Hmax blochPolar wL delwL\n";
		file << std::setprecision(std::numeric_limits<double>::digits); // need maximum precision for wL and delwL 
	}
	cout << "Printing status\n";

	value_type E = Ekin(&y[3]);
	double B[4][4], Ei[3], V;

	field->BField(ystart[0], ystart[1], ystart[2], tstart, B);
	field->EField(ystart[0], ystart[1], ystart[2], tstart, V, Ei);

	file	<< jobnumber << " " << particlenumber << " "
			<< tstart << " " << ystart[0] << " " << ystart[1] << " " << ystart[2] << " "
			<< ystart[3] << " " << ystart[4] << " " << ystart[5] << " "
			<< ystart[7] << " " << Hstart() << " " << Estart() << " "
			<< B[3][0] << " " << V << " " << solidstart.ID << " ";

	field->BField(y[0], y[1], y[2], x, B);
	field->EField(y[0], y[1], y[2], x, V, Ei);
	double polend = (spinend[0]*B[0][0] + spinend[1]*B[1][0] + spinend[2]*B[2][0])/B[3][0]/sqrt(spinend[0]*spinend[0] + spinend[1]*spinend[1] + spinend[2]*spinend[2]);

	file	<< x << " " << y[0] << " " << y[1] << " " << y[2] << " "
			<< y[3] << " " << y[4] << " " << y[5] << " "
			<< y[7] << " " << E + Epot(x, y, field, GetCurrentsolid()) << " " << E << " " // use GetCurrentsolid() for Epot, since particle may not actually have entered sld
			<< B[3][0] << " " << V << " " << sld.ID << " "
			<< ID << " " << Nspinflip << " " << 1 - noflipprob << " "
			<< Nhit << " " << Nstep << " " << lend << " " << Hmax << " " 
			<< polend << " " << wL << " " << delwL << '\n';
}


void TParticle::PrintTrack(std::ofstream &trackfile, value_type x, state_type y, solid sld){
	if (!trackfile.is_open()){
		ostringstream filename;
		filename << outpath << '/' << setw(12) << setfill('0') << jobnumber << name << "track.out";
		cout << "Creating " << filename.str() << '\n';
		trackfile.open(filename.str().c_str());
		if (!trackfile.is_open()){
			cout << "Could not create" << filename.str() << '\n';
			exit(-1);
		}
		trackfile << 	"jobnumber particle polarisation "
						"t x y z vx vy vz "
						"H E Bx dBxdx dBxdy dBxdz By dBydx "
						"dBydy dBydz Bz dBzdx dBzdy dBzdz Babs dBdx dBdy dBdz Ex Ey Ez V\n";
		trackfile.precision(10);
	}

	cout << "-";
	double B[4][4] = {{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0}};
	double E[3] = {0,0,0};
	double V = 0;
	if (field){
		field->BField(y[0],y[1],y[2],x,B);
		field->EField(y[0],y[1],y[2],x,V,E);
	}
	value_type Ek = Ekin(&y[3]);
	value_type H = Ek + Epot(x, y, field, sld);

	trackfile << jobnumber << " " << particlenumber << " " << y[7] << " "
				<< x << " " << y[0] << " " << y[1] << " " << y[2] << " " << y[3] << " " << y[4] << " " << y[5] << " "
				<< H << " " << Ek << " ";
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			trackfile << B[i][j] << " ";
	trackfile << E[0] << " " << E[1] << " " << E[2] << " " << V << '\n';
}


void TParticle::PrintHit(std::ofstream &hitfile, value_type x, state_type y1, state_type y2, const double *normal, solid *leaving, solid *entering){
	if (!hitfile.is_open()){
		ostringstream filename;
		filename << outpath << '/' << setw(12) << setfill('0') << jobnumber << name << "hit.out";
		cout << "Creating " << filename.str() << '\n';
		hitfile.open(filename.str().c_str());
		if (!hitfile.is_open()){
			cout << "Could not create" << filename.str() << '\n';
			exit(-1);
		}
		hitfile << "jobnumber particle "
					"t x y z v1x v1y v1z pol1 "
					"v2x v2y v2z pol2 "
					"nx ny nz solid1 solid2\n";
		hitfile.precision(10);
	}

	cout << ":";
	hitfile << jobnumber << " " << particlenumber << " "
			<< x << " " << y1[0] << " " << y1[1] << " " << y1[2] << " " << y1[3] << " " << y1[4] << " " << y1[5] << " " << y1[7] << " "
			<< y2[3] << " " << y2[4] << " " << y2[5] << " " << y2[7] << " "
			<< normal[0] << " " << normal[1] << " " << normal[2] << " " << leaving->ID << " " << entering->ID << '\n';
}


double TParticle::Ekin(value_type v[3]){
	value_type v2 = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	value_type beta2 = v2/c_0/c_0;
	value_type gammarel = 1/sqrt(1 - beta2); // use relativistic formula for larger beta
//			cout << "beta^8 = " << beta2*beta2*beta2*beta2 << "; err_gamma = " << numeric_limits<value_type>::epsilon()/(gammarel - 1) << '\n';
	if (beta2*beta2*beta2*beta2 < numeric_limits<value_type>::epsilon()/(gammarel - 1)) // if error in series expansion O(beta^8) is smaller than rounding error in gamma factor
		return 0.5*m*v2 + (3.0/8.0*m + (5.0/16.0*m + 35.0/128.0*beta2)*beta2)*beta2*v2; // use series expansion for energy calculation with small beta
	else{
		return c_0*c_0*m*(gammarel - 1); // else use fully relativstic formula
	}
}


double TParticle::Epot(value_type t, state_type y, TFieldManager *field, solid sld){
	value_type result = 0;
	if ((q != 0 || mu != 0) && field){
		double B[4][4], E[3], V;
		if (mu != 0){
			field->BField(y[0],y[1],y[2],t,B);
			result += -y[7]*mu/ele_e*B[3][0];
		}
		if (q != 0){
			field->EField(y[0],y[1],y[2],t,V,E);
			result += q/ele_e*V;
		}
	}
	result += m*gravconst*y[2];
	return result;
}
