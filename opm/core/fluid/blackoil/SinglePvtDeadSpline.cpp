/*
  Copyright 2010, 2011, 2012 SINTEF ICT, Applied Mathematics.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <opm/core/fluid/blackoil/SinglePvtDeadSpline.hpp>
#include <opm/core/utility/buildUniformMonotoneTable.hpp>
#include <algorithm>

// Extra includes for debug dumping of tables.
// #include <boost/lexical_cast.hpp>
// #include <string>
// #include <fstream>

namespace Opm
{

    //------------------------------------------------------------------------
    // Member functions
    //-------------------------------------------------------------------------

    /// Constructor
    SinglePvtDeadSpline::SinglePvtDeadSpline(const table_t& pvd_table, const int samples)
    {
        const int region_number = 0;
        if (pvd_table.size() != 1) {
            THROW("More than one PVT-region");
        }

        // Copy data
        const int sz = pvd_table[region_number][0].size();
        std::vector<double> press(sz);
        std::vector<double> B_inv(sz);
        std::vector<double> visc(sz);
        for (int i = 0; i < sz; ++i) {
            press[i] = pvd_table[region_number][0][i];
            B_inv[i] = 1.0 / pvd_table[region_number][1][i];
            visc[i]  = pvd_table[region_number][2][i];
        }
        buildUniformMonotoneTable(press, B_inv, samples, one_over_B_);
        buildUniformMonotoneTable(press, visc, samples, viscosity_);

        // Dumping the created tables.
//         static int count = 0;
//         std::ofstream os((std::string("dump-") + boost::lexical_cast<std::string>(count++)).c_str());
//         os.precision(15);
//         os << "1/B\n\n" << one_over_B_
//            << "\n\nvisc\n\n" << viscosity_ << std::endl;
    }

    // Destructor
    SinglePvtDeadSpline::~SinglePvtDeadSpline()
    {
    }



    void SinglePvtDeadSpline::mu(const int n,
                                 const double* p,
                                 const double* /*z*/,
                                 double* output_mu) const
    {
// #pragma omp parallel for
        for (int i = 0; i < n; ++i) {
            output_mu[i] = viscosity_(p[i]);
        }
    }

    void SinglePvtDeadSpline::B(const int n,
                                const double* p,
                                const double* /*z*/,
                                double* output_B) const
    {
// #pragma omp parallel for
        for (int i = 0; i < n; ++i) {
            output_B[i] = 1.0/one_over_B_(p[i]);
        }
    }

    void SinglePvtDeadSpline::dBdp(const int n,
                                   const double* p,
                                   const double* /*z*/,
                                   double* output_B,
                                   double* output_dBdp) const
    {
        B(n, p, 0, output_B);
// #pragma omp parallel for
        for (int i = 0; i < n; ++i) {
            double Bg = output_B[i];
            output_dBdp[i] = -Bg*Bg*one_over_B_.derivative(p[i]);
        }
    }


    void SinglePvtDeadSpline::R(const int n,
                                const double* /*p*/,
                                const double* /*z*/,
                                double* output_R) const
    {
        std::fill(output_R, output_R + n, 0.0);
    }

    void SinglePvtDeadSpline::dRdp(const int n,
                                   const double* /*p*/,
                                   const double* /*z*/,
                                   double* output_R,
                                   double* output_dRdp) const
    {
        std::fill(output_R, output_R + n, 0.0);
        std::fill(output_dRdp, output_dRdp + n, 0.0);
    }

}
