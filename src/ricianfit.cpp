//
// Copyright (c) 2016 Enrico Kaden & University College London
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

#include "docopt.h"

#include "darray.h"
#include "nifti.h"
#include "ricianfit.h"
#include "sarray.h"
#include "version.h"

static const char VERSION[] = R"(ricianfit)" " " STR(SMT_VERSION_STRING);

static const char LICENSE[] = R"(
Copyright (c) 2016 Enrico Kaden & University College London
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
)";

static const char USAGE[] = R"(
RICIAN NOISE ESTIMATION

Copyright (c) 2016 Enrico Kaden & University College London

Usage:
  ricianfit [options] <input> <output>
  ricianfit (-h | --help)
  ricianfit --license
  ricianfit --version

Options:
  --mask <mask>  Foreground mask [default: none]
  -h, --help     Help screen
  --license      License information
  --version      Software version
)";

template <typename float_t>
smt::inifti<float_t, 3> read_mask(std::map<std::string, docopt::value>& args) {
	if(args["--mask"] && args["--mask"].asString() != "none") {
		return smt::inifti<float_t, 3>(args["--mask"].asString());
	} else {
		return smt::inifti<float_t, 3>();
	}
}

int main(int argc, const char** argv) {

	typedef double float_t;

	// Input

	std::map<std::string, docopt::value> args = docopt::docopt(USAGE, {argv+1, argv+argc}, true, VERSION);
	if(args["--license"].asBool()) {
		std::cout << LICENSE << std::endl;
		return EXIT_SUCCESS;
	}

	const smt::inifti<float_t, 4> input(args["<input>"].asString());
	if(input.size(3) < 2) {
		std::cerr << "ERROR: '" << args["<input>"].asString() << "' includes less than two volumes." << std::endl;
		return EXIT_FAILURE;
	}

	const smt::inifti<float_t, 3> mask = read_mask<float_t>(args);
	if(mask) {
		if(input.size(0) != mask.size(0) || input.size(1) != mask.size(1) || input.size(2) != mask.size(2)) {
			std::cerr << "ERROR: '" << args["<input>"].asString() << "' and '" << args["--mask"].asString() << "' do not match." << std::endl;
			return EXIT_FAILURE;
		}
		if(input.pixsize(0) != mask.pixsize(0) || input.pixsize(1) != mask.pixsize(1) || input.pixsize(2) != mask.pixsize(2)) {
			std::cerr << "ERROR: The pixel sizes of '" << args["<input>"].asString() << "' and '" << args["--mask"].asString() << "' do not match." << std::endl;
			return EXIT_FAILURE;
		}
		if(! input.has_equal_spatial_coords(mask)) {
			std::cerr << "ERROR: The coordinate systems of '" << args["<input>"].asString() << "' and '" << args["--mask"].asString() << "' do not match." << std::endl;
			return EXIT_FAILURE;
		}
	}

	// Processing

	// TODO: Separate NIfTI-1 output (e.g. <output>_signal.nii and <output>_noise.nii).
	smt::onifti<float, 4> output = smt::onifti<float, 4>(args["<output>"].asString(), input, input.size(0), input.size(1), input.size(2), 2);

	const std::size_t input_size_0 = input.size(0);
	const std::size_t input_size_1 = input.size(1);
	const std::size_t input_size_2 = input.size(2);
#pragma omp parallel for schedule(dynamic, 10) collapse(3)
	for(std::size_t kk = 0; kk < input_size_2; ++kk) {
		for(std::size_t jj = 0; jj < input_size_1; ++jj) {
			for(std::size_t ii = 0; ii < input_size_0; ++ii) {
				if((! mask) || mask(ii, jj, kk) > 0) {
					smt::darray<float_t, 1> input_tmp = input(ii, jj, kk, smt::slice(0, input.size(3)));

					const smt::sarray<float_t, 2> fit = smt::ricianfit(input_tmp);
					output(ii, jj, kk, 0) = fit(0);
					output(ii, jj, kk, 1) = fit(1);
				} else {
					output(ii, jj, kk, 0) = 0;
					output(ii, jj, kk, 1) = 0;
				}
			}
		}
	}

	return EXIT_SUCCESS;
}
