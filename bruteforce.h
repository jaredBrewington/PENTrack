#include <vector>

using namespace std;

long double BruteForceIntegration(long double x1, VecDoub_I &y1, long double B1[4][4], long double x2, VecDoub_I &y2, long double B2[4][4], long double xmax); // integrate spin flip probability

// variables for BruteForce Bloch integration BEGIN
extern long double BFTargetB;     // smallest value of Babs during step, Babs < BFTargetB => integrate,
extern int flipspin;
extern vector<long double> BFtimes;

