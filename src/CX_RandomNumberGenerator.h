#ifndef _CX_RANDOM_NUMBER_GENERATOR_H_
#define _CX_RANDOM_NUMBER_GENERATOR_H_

#include <random>
#include <vector>
#include <set>

#include <stdint.h>

#include "CX_Utilities.h"

namespace CX {

	typedef int64_t CX_RandomInt_t;

	class CX_RandomNumberGenerator {
	public:

		CX_RandomNumberGenerator (void);

		void setSeed (uint64_t seed);
		uint64_t getSeed (void) { return _seed; };
	
		CX_RandomInt_t getMinimumRandomInt(void);
		CX_RandomInt_t getMaximumRandomInt(void);
	
		CX_RandomInt_t randomInt(void);
		CX_RandomInt_t randomInt(CX_RandomInt_t rangeLower, CX_RandomInt_t rangeUpper);

		template <typename T> T randomExclusive (const vector<T> &values, const T& exclude);
		template <typename T> T randomExclusive (const vector<T> &values, const vector<T> &exclude);

		//double uniformDouble(double lowerBound_closed, double upperBound_open);
		std::vector<double> uniformDeviates (unsigned int count, double lowerBound_closed, double upperBound_open);
		template <typename T> std::vector<T> binomialDeviates (unsigned int count, T trials, double probSuccess);
		std::vector<double> normalDeviates (unsigned int count, double mean, double standardDeviation);

		template <typename T> void shuffleVector (vector<T> *v);
		template <typename T> vector<T> shuffleVector (vector<T> v);
		template <typename T> T sample (vector<T> values);
		template <typename T> vector<T> sample (unsigned int count, const vector<T> &source, bool withReplacement);
		vector<int> sample (unsigned int count, int lowerBound, int upperBound, bool withReplacement);

	private:
		uint64_t _seed;

		std::mt19937_64 _mersenneTwister;
	};

	namespace Instances {
		extern CX_RandomNumberGenerator RNG;
	}

	/*! Randomizes the order of the given vector.
	\param v A pointer to the vector to be shuffled. */
	template <typename T>
	void CX_RandomNumberGenerator::shuffleVector (vector<T> *v) {
		std::shuffle( v->begin(), v->end(), _mersenneTwister );
	}

	/*! Makes a copy of the given vector, randomizes the order of its elements, and returns the shuffled copy.
	\param v The vector to be operated on. 
	\return A shuffled copy of v. */
	template <typename T>
	vector<T> CX_RandomNumberGenerator::shuffleVector (vector<T> v) {
		std::shuffle( v.begin(), v.end(), _mersenneTwister );
		return v;
	}

	/*!	Returns a vector of count values drawn randomly from source, with or without replacement. The returned values are in a random order.
	\param count The number of samples to draw.
	\param source A vector to be sampled from.
	\param withReplacement Sample with or without replacement.
	\return A vector of the sampled values.
	\note If (count > source.size() && withReplacement == false), an empty vector is returned.
	*/
	template <typename T>
	vector<T> CX_RandomNumberGenerator::sample (unsigned int count, const vector<T> &source, bool withReplacement) {

		vector<T> samples;

		if (source.size() == 0) {
			Instances::Log.error("CX_RandomNumberGenerator") << "sample: Empty vector given to sample from.";
			return samples;
		}

		if (withReplacement) {
			for (vector<T>::size_type i = 0; i < count; i++) {
				samples.push_back( source.at( (vector<CX_RandomInt_t>::size_type)randomInt(0, source.size() - 1) ) );
			}
		} else {
			//Without replacement. Make a vector of indices into the source vector, shuffle them, and select count of them from the vector.
			if (count > source.size()) {
				//Log a warning?
				return samples;
			}
			vector<vector<T>::size_type> indices = shuffleVector( CX::intVector<vector<T>::size_type>(0, source.size() - 1) );
			for (unsigned int i = 0; i < count; i++) {
				samples.push_back( source[ indices[i] ] );
			}
		}

		return samples;
	}

	/*! Returns a single value sampled randomly from values.
	\return The sampled value.
	\note If values.size() == 0, an error will be logged and T() will be returned.
	*/
	template <typename T> T CX_RandomNumberGenerator::sample (vector<T> values) {
		if (values.size() == 0) {
			Instances::Log.error("CX_RandomNumberGenerator") << "sample: Empty vector given to sample from.";
			return T();
		}
		return values[ randomInt(0, values.size() - 1) ];
	}

	template <typename T> T CX_RandomNumberGenerator::randomExclusive (const vector<T> &values, const T &exclude) {
		std::vector<T> excludes(1, exclude);
		return randomExclusive(values, excludes);
	}

	template <typename T> T CX_RandomNumberGenerator::randomExclusive (const vector<T> &values, const vector<T> &exclude) {
		std::set<T> s(exclude.begin(), exclude.end());

		if (values.size() == exclude.size()) {
			bool allExcluded = true;
			for (std::vector<T>::size_type i = 0; i < values.size(); i++) {
				if (s.find(values[i]) == s.end()) {
					allExcluded = allExcluded && false;
				}
			}

			if (allExcluded) {
				Log.error("CX_RandomNumberGenerator") << "randomExclusive: All values are excluded.";
				return T();
			}
		}

		T attempt;
		do {
			attempt = sample(values);
		} while (s.find(attempt) != s.end());
		return attempt;
	}

	/*!	Samples count deviates from a binomial distribution with the given number of trials and probability of success on each trial.
	\param count The number of deviates to generate.
	\param trials The number of trials. Must be a non-negative integer.
	\param probSuccess The probability of a success on a given trial, where a success is the value 1.
	\return A vector of the deviates. */
	template <typename T> std::vector<T> CX_RandomNumberGenerator::binomialDeviates (unsigned int count, T trials, double probSuccess) {
		std::vector<T> samples(count);
		std::binomial_distribution<T> binDist(trials, probSuccess);
		for (uint64_t i = 0; i < count; i++) {
			samples[i] = binDist(_mersenneTwister);
		}
		return samples;
	}



}

#endif //_CX_RANDOM_NUMBER_GENERATOR_H_