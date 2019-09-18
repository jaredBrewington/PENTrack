#ifndef GLOBALS_H_
#define GLOBALS_H_

#include <string>
#include <vector>
#include <map>
#include <string>
#include <iostream>

#include <boost/filesystem.hpp>

enum stopID {	ID_UNKNOWN = 0, ///< standard flag for particles
				ID_NOT_FINISH = -1, ///< flag for particles which reached ::StorageTime
				ID_HIT_BOUNDARIES = -2, ///< flag for particles which left bounding box of TParticle::geom
				ID_ODEINT_ERROR = -3, ///< flag for particles which produced a numerical error during ODe integration
				ID_DECAYED = -4, ///< flag for particles which reached TParticle::tau
				ID_INITIAL_NOT_FOUND = -5, ///< flag for particles which had a too low total energy to find a initial spot in the source volume
				ID_CGAL_ERROR = -6, ///< flag for particles which produced an error during geometry collision checks
				ID_GEOMETRY_ERROR = -7, ///< flag for particles which produced an error while tracking material boundaries along the trajectory
				ID_ABSORBED_IN_MATERIAL = 1, ///< flag for particles that were absorbed inside a material
				ID_ABSORBED_ON_SURFACE = 2 ///< flag for particles that were absorbed on a material surface
};

enum simType {	PARTICLE = 1, ///< set simtype in configuration to this value to simulate particles
				BF_ONLY = 3, ///< set simtype in configuration to this value to print out a ramp heating analysis
				BF_CUT = 4, ///< set simtype in configuration to this value to print out a planar slice through electric/magnetic fields
				GEOMETRY = 7, ///< set simtype in configuration to this value to print out a sampling of the geometry
				MR_THETA_OUT_ANGLE = 8, ///< set simtype in configuration to this value to output a 3d histogram of the MR model's diffuse reflection probability for every solid angle
				MR_THETA_I_ENERGY = 9 ///< set simtype in configuration to this value to output a 3d histogram of the MR models' diffuse reflection probability for theta_i vs neutron energy
};

// physical constants
extern const long double pi; ///< Pi
extern const long double ele_e; ///< elementary charge [C]
extern const long double gravconst; ///< g [m/s]
extern const long double boltzconst; /// Boltzmann's constant [ev/K] from http://physics.nist.gov/cgi-bin/cuu/Value?sigma
extern const long double avogadroconst; /// Avogadro's constant [ mol^-1 ] from http://physics.nist.gov/cgi-bin/cuu/Value?na|search_for=avagadro%27s+number
extern const long double conv; ///< deg to rad conversion factor
extern const long double mu0; ///< magnetic permeability [Vs/Am]
extern const long double m_n; ///< neutron mass [eV/c^2]
extern const long double m_p; ///< proton mass [eV/c^2]
extern const long double m_e; ///< electron mass [eV/c^2]
extern const long double m_hg; ///< mercury-199 mass [ev/c^2] (http://physics.nist.gov/cgi-bin/Compositions/stand_alone.pl?ele=Hg)
extern const long double m_xe; ///< xenon-129 mass [ev/c^2] (http://physics.nist.gov/cgi-bin/Compositions/stand_alone.pl?ele=Xe)
extern const long double c_0; ///< light speed [m/s]
extern const long double hbar; ///< planck constant [Js]
extern const long double mu_nSI;	///< Neutron magnetic moment [J/T]
extern const long double mu_hgSI; ///< mercury-199 magnetic moment [J/T]
extern const long double mu_xeSI; ///< Xenon-129 avogadroconstmagnetic moment [J/T]
extern const long double gamma_n; ///< 2*::mu_nSI/::hquer gyromagnetic ratio of neutron [1/Ts]
extern const long double gamma_hg; ///< from: http://www.sciencedirect.com/science/article/pii/S0370269314007692 [ 1/Ts ]
extern const long double gamma_xe; ///< from: http://nmrwiki.org/wiki/index.php?title=Gyromagnetic_ratio [ 1/Ts ]

extern long long int jobnumber; ///< job number, read from command line paramters, used for parallel calculations
extern boost::filesystem::path configpath; ///< path to configuration file, read from command line paramters
extern boost::filesystem::path outpath; ///< path where the log file should be saved to, read from command line parameters

class progress_display{
private:
    unsigned long _count;
    unsigned long _expected_count;
    std::ostream& _os;
    std::string _s1;

    void print_progress(){
        _os << '\r' << _s1;
        for (unsigned long i = 0; i <= 100; ++i) {
            if (i == 0)
                _os << '[';
            else if (i == 100)
                _os << ']';
            else if (i % 10 == 0)
                _os << '|';
            else if (i > 100 * _count / _expected_count)
                _os << ' ';
            else
                _os << '-';
        }
        _os.flush();
    }
public:
    progress_display( unsigned long expected_count ): _os(std::cout){
        restart(expected_count);
    }
    // Effects: restart(expected_count)

    progress_display( unsigned long expected_count,
                      std::ostream& os,
                      const std::string & s1 = ""): _os(std::cout){
        _s1 = s1;
        restart(expected_count);
    }
    // Effects: save copy of leading strings, restart(expected_count)

    void restart( unsigned long expected_count ){
        _expected_count = expected_count;
        _count = 0;
        print_progress();
    }
    //  Effects: display appropriate scale on three lines,
    //  prefaced by stored copy of s1, s2, s3, respectively, from constructor
    //  Postconditions: count()==0, expected_count()==expected_count

    unsigned long operator+=( unsigned long increment ){
        unsigned long acount = _count;
        _count += increment;
        if (100*_count/_expected_count > 100*acount/_expected_count)
            print_progress();
        return _count;
    }
    //  Effects: Display appropriate progress tic if needed.
    //  Postconditions: count()== original count() + increment
    //  Returns: count().

    unsigned long operator++(){
        return operator+=(1);
    }
    //  Returns: operator+=( 1 ).

    unsigned long count() const{
        return _count;
    }
    //  Returns: The internal count.

    unsigned long expected_count() const{
        return _expected_count;
    }
    //  Returns: The expected_count from the constructor.

}; // progress_display

/**
 * Rotate a vector.
 *
 * Rotate vector into new coordinate with basis vectors x and z (active transformation)
 */
void RotateVector(double v[3], const double z[3], const double x[3] = NULL);

/**
 * Lorentz boost of four-vector p into frame moving in arbitrary direction with v/c = beta.
 *
 * Copy & paste from ROOT
 *
 * @param beta Vector v/c of moving reference frame
 * @param p Four-vector
 */
void BOOST(const std::vector<double> &beta, std::vector<double> &p);

/**
 * Energy distribution of protons from free neutron beta decay (0 < E < 750 eV)
 *
 * From diploma thesis M. Simson.
 *
 * @param E energy [eV]
 *
 * @return Returns a probability between 0 and 1 that a proton with energy E is created.
 */
double ProtonBetaSpectrum(const double E);


/**
 * Energy distribution of electrons from free neutron decay (0 < E < 782 keV)
 *
 * From "http://hyperphysics.phy-astr.gsu.edu/Hbase/nuclear/beta2.html"
 *
 * @param E energy [eV]
 *
 * @return Returns probability between 0 and 1 that an electron with energy E is created.
 */
double ElectronBetaSpectrum(const double E);

/**
 * Energy distribution of comagnetometer gasses from Maxwell-Boltzmann distribution. 
 *
 * From "http://hyperphysics.phy-astr.gsu.edu/hbase/kinetic/maxspe.html"
 *
 * @param T temperature of the gas in Kelvin
 * @param E energy in eV
 *
 * @return Returns probability between 0 and 1 that a gas molecule with energy E being created. 
 */
double MaxwellBoltzSpectrum (const double T, const double E);

#endif /*GLOBALS_H_*/
