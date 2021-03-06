/*
  Copyright 2014 SINTEF ICT, Applied Mathematics.
*/

#include "config.h"

/* --- Boost.Test boilerplate --- */
#if HAVE_DYNAMIC_BOOST_TEST
#define BOOST_TEST_DYN_LINK
#endif

#define NVERBOSE  // Suppress own messages when throw()ing

#define BOOST_TEST_MODULE UnitsTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>

/* --- our own headers --- */

#include <opm/core/simulator/initStateEquil.hpp>

#include <opm/core/grid.h>
#include <opm/core/grid/cart_grid.h>
#include <opm/core/grid/GridManager.hpp>

#include <opm/core/props/BlackoilPropertiesBasic.hpp>
#include <opm/core/props/BlackoilPropertiesFromDeck.hpp>
#include <opm/core/props/BlackoilPhases.hpp>

#include <opm/parser/eclipse/Parser/Parser.hpp>
#include <opm/parser/eclipse/Parser/ParseContext.hpp>
#include <opm/parser/eclipse/Deck/Deck.hpp>
#include <opm/parser/eclipse/Deck/DeckItem.hpp>
#include <opm/parser/eclipse/Deck/DeckRecord.hpp>
#include <opm/parser/eclipse/EclipseState/InitConfig/Equil.hpp>
#include <opm/parser/eclipse/Units/Dimension.hpp>

#include <opm/core/pressure/msmfem/partition.h>

#include <opm/core/utility/parameters/ParameterGroup.hpp>
#include <opm/parser/eclipse/Units/Units.hpp>

#include <array>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#define CHECK(value, expected, reltol) \
{ \
  if (std::fabs((expected)) < 1.e-14) \
    BOOST_CHECK_SMALL((value), (reltol)); \
  else \
    BOOST_CHECK_CLOSE((value), (expected), (reltol)); \
}

BOOST_AUTO_TEST_SUITE ()

static Opm::EquilRecord mkEquilRecord( double datd, double datp,
                                       double zwoc, double pcow_woc,
                                       double zgoc, double pcgo_goc ) {
    using namespace Opm;

    DeckItem dd( "datdep", double() );
    dd.push_back( datd  );
    Opm::Dimension dd_dim( "dddim", 1 );
    dd.push_backDimension( dd_dim, dd_dim );

    DeckItem dp( "datps", double() );
    dp.push_back( datp );
    Opm::Dimension dp_dim( "dpdim", 1 );
    dp.push_backDimension( dp_dim, dp_dim );

    DeckItem zw( "zwoc", double() );
    zw.push_back( zwoc );
    Opm::Dimension zw_dim( "zwdim", 1 );
    zw.push_backDimension( zw_dim, zw_dim );

    DeckItem pcow( "pcow", double() );
    pcow.push_back( pcow_woc );
    Opm::Dimension pcow_dim( "pcowdim", 1 );
    pcow.push_backDimension( pcow_dim, pcow_dim );

    DeckItem zg( "zgoc", double() );
    zg.push_back( zgoc );
    Opm::Dimension zg_dim( "zgdim", 1 );
    zg.push_backDimension( zg_dim, zg_dim );

    DeckItem pcgo( "pcgo", double() );
    pcgo.push_back( pcgo_goc );
    Opm::Dimension pcgo_dim( "pcgodim", 1 );
    pcgo.push_backDimension( pcgo_dim, pcgo_dim );

    DeckItem i1( "i1", int() );
    DeckItem i2( "i2", int() );
    DeckItem i3( "i3", int() );
    i1.push_back( 0 );
    i2.push_back( 0 );
    i3.push_back( 0 );

    DeckRecord rec;
    rec.addItem( std::move( dd ) );
    rec.addItem( std::move( dp ) );
    rec.addItem( std::move( zw ) );
    rec.addItem( std::move( pcow ) );
    rec.addItem( std::move( zg ) );
    rec.addItem( std::move( pcgo ) );
    rec.addItem( std::move( i1 ) );
    rec.addItem( std::move( i2 ) );
    rec.addItem( std::move( i3 ) );

    return EquilRecord( rec );
}

BOOST_AUTO_TEST_CASE (PhasePressure)
{
    typedef std::vector<double> PVal;
    typedef std::vector<PVal> PPress;

    std::shared_ptr<UnstructuredGrid>
        G(create_grid_cart3d(10, 1, 10), destroy_grid);

    Opm::parameter::ParameterGroup param;
    {
        using Opm::unit::kilogram;
        using Opm::unit::meter;
        using Opm::unit::cubic;

        std::stringstream dens; dens << 700*kilogram/cubic(meter);
        param.insertParameter("rho2", dens.str());
    }

    typedef Opm::BlackoilPropertiesBasic Props;
    Props props(param, G->dimensions, G->number_of_cells);

    typedef Opm::EQUIL::DensityCalculator<Opm::BlackoilPropertiesInterface> RhoCalc;
    RhoCalc calc(props, 0);

    auto record = mkEquilRecord( 0, 1e5, 5, 0, 0, 0 );

    Opm::EQUIL::EquilReg<RhoCalc>
        region(record, calc,
               std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
               std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
               props.phaseUsage());

    std::vector<int> cells(G->number_of_cells);
    std::iota(cells.begin(), cells.end(), 0);

    const double grav   = 10;
    const PPress ppress = Opm::EQUIL::phasePressures(*G, region, cells, grav);

    const int first = 0, last = G->number_of_cells - 1;
    const double reltol = 1.0e-8;
    BOOST_CHECK_CLOSE(ppress[0][first] ,  90e3   , reltol);
    BOOST_CHECK_CLOSE(ppress[0][last ] , 180e3   , reltol);
    BOOST_CHECK_CLOSE(ppress[1][first] , 103.5e3 , reltol);
    BOOST_CHECK_CLOSE(ppress[1][last ] , 166.5e3 , reltol);
}

BOOST_AUTO_TEST_CASE (CellSubset)
{
    typedef std::vector<double> PVal;
    typedef std::vector<PVal> PPress;

    std::shared_ptr<UnstructuredGrid>
        G(create_grid_cart3d(10, 1, 10), destroy_grid);

    Opm::parameter::ParameterGroup param;
    {
        using Opm::unit::kilogram;
        using Opm::unit::meter;
        using Opm::unit::cubic;

        std::stringstream dens; dens << 700*kilogram/cubic(meter);
        param.insertParameter("rho2", dens.str());
    }

    typedef Opm::BlackoilPropertiesBasic Props;
    Props props(param, G->dimensions, G->number_of_cells);

    typedef Opm::EQUIL::DensityCalculator<Opm::BlackoilPropertiesInterface> RhoCalc;
    RhoCalc calc(props, 0);

    Opm::EquilRecord record[] = { mkEquilRecord( 0, 1e5, 2.5, -0.075e5, 0, 0 ),
                                  mkEquilRecord( 5, 1.35e5, 7.5, -0.225e5, 5, 0 ) };

    Opm::EQUIL::EquilReg<RhoCalc> region[] =
        {
            Opm::EQUIL::EquilReg<RhoCalc>(record[0], calc,
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          props.phaseUsage())
            ,
            Opm::EQUIL::EquilReg<RhoCalc>(record[0], calc,
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          props.phaseUsage())
            ,
            Opm::EQUIL::EquilReg<RhoCalc>(record[1], calc,
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          props.phaseUsage())
            ,
            Opm::EQUIL::EquilReg<RhoCalc>(record[1], calc,
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          props.phaseUsage())
        };

    const int cdim[] = { 2, 1, 2 };
    int ncoarse = cdim[0];
    for (std::size_t d = 1; d < 3; ++d) { ncoarse *= cdim[d]; }

    std::vector< std::vector<int> > cells(ncoarse);
    for (int c = 0; c < G->number_of_cells; ++c) {
        int ci = c;
        const int i = ci % G->cartdims[0];  ci /= G->cartdims[0];
        const int j = ci % G->cartdims[1];
        const int k = ci / G->cartdims[1];

        const int ic = (i / (G->cartdims[0] / cdim[0]));
        const int jc = (j / (G->cartdims[1] / cdim[1]));
        const int kc = (k / (G->cartdims[2] / cdim[2]));
        const int ix = ic + cdim[0]*(jc + cdim[1]*kc);

        assert ((0 <= ix) && (ix < ncoarse));
        cells[ix].push_back(c);
    }

    PPress ppress(2, PVal(G->number_of_cells, 0));
    for (std::vector< std::vector<int> >::const_iterator
             r = cells.begin(), e = cells.end();
         r != e; ++r)
    {
        const int    rno  = int(r - cells.begin());
        const double grav = 10;
        const PPress p    =
            Opm::EQUIL::phasePressures(*G, region[rno], *r, grav);

        PVal::size_type i = 0;
        for (std::vector<int>::const_iterator
                 c = r->begin(), ce = r->end();
             c != ce; ++c, ++i)
        {
            assert (i < p[0].size());

            ppress[0][*c] = p[0][i];
            ppress[1][*c] = p[1][i];
        }
    }

    const int first = 0, last = G->number_of_cells - 1;
    const double reltol = 1.0e-8;
    BOOST_CHECK_CLOSE(ppress[0][first] , 105e3   , reltol);
    BOOST_CHECK_CLOSE(ppress[0][last ] , 195e3   , reltol);
    BOOST_CHECK_CLOSE(ppress[1][first] , 103.5e3 , reltol);
    BOOST_CHECK_CLOSE(ppress[1][last ] , 166.5e3 , reltol);
}




BOOST_AUTO_TEST_CASE (RegMapping)
{
    typedef std::vector<double> PVal;
    typedef std::vector<PVal> PPress;

    std::shared_ptr<UnstructuredGrid>
        G(create_grid_cart3d(10, 1, 10), destroy_grid);

    Opm::parameter::ParameterGroup param;
    {
        using Opm::unit::kilogram;
        using Opm::unit::meter;
        using Opm::unit::cubic;

        std::stringstream dens; dens << 700*kilogram/cubic(meter);
        param.insertParameter("rho2", dens.str());
    }

    typedef Opm::BlackoilPropertiesBasic Props;
    Props props(param, G->dimensions, G->number_of_cells);

    typedef Opm::EQUIL::DensityCalculator<Opm::BlackoilPropertiesInterface> RhoCalc;
    RhoCalc calc(props, 0);

    Opm::EquilRecord record[] = { mkEquilRecord( 0, 1e5, 2.5, -0.075e5, 0, 0 ),
                                  mkEquilRecord( 5, 1.35e5, 7.5, -0.225e5, 5, 0 ) };

    Opm::EQUIL::EquilReg<RhoCalc> region[] =
        {
            Opm::EQUIL::EquilReg<RhoCalc>(record[0], calc,
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          props.phaseUsage())
            ,
            Opm::EQUIL::EquilReg<RhoCalc>(record[0], calc,
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          props.phaseUsage())
            ,
            Opm::EQUIL::EquilReg<RhoCalc>(record[1], calc,
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          props.phaseUsage())
            ,
            Opm::EQUIL::EquilReg<RhoCalc>(record[1], calc,
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          std::make_shared<Opm::EQUIL::Miscibility::NoMixing>(),
                                          props.phaseUsage())
        };

    std::vector<int> eqlnum(G->number_of_cells);
    {
        std::vector<int> cells(G->number_of_cells);
        std::iota(cells.begin(), cells.end(), 0);

        const int cdim[] = { 2, 1, 2 };
        int ncoarse = cdim[0];
        for (std::size_t d = 1; d < 3; ++d) { ncoarse *= cdim[d]; }

        partition_unif_idx(G->dimensions, G->number_of_cells,
                           G->cartdims, cdim,
                           &cells[0], &eqlnum[0]);
    }
    Opm::RegionMapping<> eqlmap(eqlnum);

    PPress ppress(2, PVal(G->number_of_cells, 0));
    for (const auto& r : eqlmap.activeRegions()) {
        const auto& rng = eqlmap.cells(r);

        const int    rno  = r;
        const double grav = 10;
        const PPress p    =
            Opm::EQUIL::phasePressures(*G, region[rno], rng, grav);

        PVal::size_type i = 0;
        for (const auto& c : rng) {
            assert (i < p[0].size());

            ppress[0][c] = p[0][i];
            ppress[1][c] = p[1][i];

            ++i;
        }
    }

    const int first = 0, last = G->number_of_cells - 1;
    const double reltol = 1.0e-8;
    BOOST_CHECK_CLOSE(ppress[0][first] , 105e3   , reltol);
    BOOST_CHECK_CLOSE(ppress[0][last ] , 195e3   , reltol);
    BOOST_CHECK_CLOSE(ppress[1][first] , 103.5e3 , reltol);
    BOOST_CHECK_CLOSE(ppress[1][last ] , 166.5e3 , reltol);
}



BOOST_AUTO_TEST_CASE (DeckAllDead)
{
    std::shared_ptr<UnstructuredGrid>
        grid(create_grid_cart3d(1, 1, 10), destroy_grid);
    Opm::ParseContext parseContext;
    Opm::Parser parser;
    Opm::Deck deck = parser.parseFile("deadfluids.DATA" , parseContext);
    Opm::EclipseState eclipseState(deck, parseContext);
    Opm::BlackoilPropertiesFromDeck props(deck, eclipseState, *grid, false);
    Opm::EQUIL::DeckDependent::InitialStateComputer comp(props, deck, eclipseState, *grid, 10.0);
    const auto& pressures = comp.press();
    BOOST_REQUIRE(pressures.size() == 3);
    BOOST_REQUIRE(int(pressures[0].size()) == grid->number_of_cells);

    const int first = 0, last = grid->number_of_cells - 1;
    // The relative tolerance is too loose to be very useful,
    // but the answer we are checking is the result of an ODE
    // solver, and it is unclear if we should check it against
    // the true answer or something else.
    const double reltol = 1.0e-3;
    BOOST_CHECK_CLOSE(pressures[0][first] , 1.496329839e7   , reltol);
    BOOST_CHECK_CLOSE(pressures[0][last ] , 1.504526940e7   , reltol);
    BOOST_CHECK_CLOSE(pressures[1][last] , 1.504526940e7   , reltol);
}



BOOST_AUTO_TEST_CASE (CapillaryInversion)
{
    // Test setup.
    Opm::GridManager gm(1, 1, 40, 1.0, 1.0, 2.5);
    const UnstructuredGrid& grid = *(gm.c_grid());
    Opm::Parser parser;
    Opm::ParseContext parseContext;
    Opm::Deck deck = parser.parseFile("capillary.DATA" , parseContext);
    Opm::EclipseState eclipseState(deck , parseContext);
    Opm::BlackoilPropertiesFromDeck props(deck, eclipseState, grid, false);

    // Test the capillary inversion for oil-water.
    const int cell = 0;
    const double reltol = 1.0e-7;
    {
        const int phase = 0;
        const bool increasing = false;
        const std::vector<double> pc = { 10.0e5, 0.5e5, 0.4e5, 0.3e5, 0.2e5, 0.1e5, 0.099e5, 0.0e5, -10.0e5 };
        const std::vector<double> s = { 0.2, 0.2, 0.2, 0.466666666666, 0.733333333333, 1.0, 1.0, 1.0, 1.0 };
        BOOST_REQUIRE(pc.size() == s.size());
        for (size_t i = 0; i < pc.size(); ++i) {
            const double s_computed = Opm::EQUIL::satFromPc(props, phase, cell, pc[i], increasing);
            BOOST_CHECK_CLOSE(s_computed, s[i], reltol);
        }
    }

    // Test the capillary inversion for gas-oil.
    {
        const int phase = 2;
        const bool increasing = true;
        const std::vector<double> pc = { 10.0e5, 0.6e5, 0.5e5, 0.4e5, 0.3e5, 0.2e5, 0.1e5, 0.0e5, -10.0e5 };
        const std::vector<double> s = { 0.8, 0.8, 0.8, 0.533333333333, 0.266666666666, 0.0, 0.0, 0.0, 0.0 };
        BOOST_REQUIRE(pc.size() == s.size());
        for (size_t i = 0; i < pc.size(); ++i) {
            const double s_computed = Opm::EQUIL::satFromPc(props, phase, cell, pc[i], increasing);
            BOOST_CHECK_CLOSE(s_computed, s[i], reltol);
        }
    }

    // Test the capillary inversion for gas-water.
    {
        const int water = 0;
        const int gas = 2;
        const std::vector<double> pc = { 0.9e5, 0.8e5, 0.6e5, 0.4e5, 0.3e5 };
        const std::vector<double> s = { 0.2, 0.333333333333, 0.6, 0.866666666666, 1.0 };
        BOOST_REQUIRE(pc.size() == s.size());
        for (size_t i = 0; i < pc.size(); ++i) {
            const double s_computed = Opm::EQUIL::satFromSumOfPcs(props, water, gas, cell, pc[i]);
            BOOST_CHECK_CLOSE(s_computed, s[i], reltol);
        }
    }
}



BOOST_AUTO_TEST_CASE (DeckWithCapillary)
{
    Opm::GridManager gm(1, 1, 20, 1.0, 1.0, 5.0);
    const UnstructuredGrid& grid = *(gm.c_grid());
    Opm::Parser parser;
    Opm::ParseContext parseContext;
    Opm::Deck deck = parser.parseFile("capillary.DATA" , parseContext);
    Opm::EclipseState eclipseState(deck , parseContext);
    Opm::BlackoilPropertiesFromDeck props(deck, eclipseState, grid, false);

    Opm::EQUIL::DeckDependent::InitialStateComputer comp(props, deck, eclipseState, grid, 10.0);
    const auto& pressures = comp.press();
    BOOST_REQUIRE(pressures.size() == 3);
    BOOST_REQUIRE(int(pressures[0].size()) == grid.number_of_cells);

    const int first = 0, last = grid.number_of_cells - 1;
    // The relative tolerance is too loose to be very useful,
    // but the answer we are checking is the result of an ODE
    // solver, and it is unclear if we should check it against
    // the true answer or something else.
    const double reltol = 1.0e-6;
    BOOST_CHECK_CLOSE(pressures[0][first] , 1.469769063e7   , reltol);
    BOOST_CHECK_CLOSE(pressures[0][last ] , 15452880.328284413   , reltol);
    BOOST_CHECK_CLOSE(pressures[1][last] , 15462880.328284413   , reltol);

    const auto& sats = comp.saturation();
    const std::vector<double> s[3]{
        { 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.42192000000000002, 0.77802666666666664, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0, 0, 0, 0.00736, 0.792746666666, 0.8, 0.8, 0.8, 0.8, 0.57807999999999993, 0.22197333333333336, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0.8, 0.8, 0.8, 0.79264, 0.007253333333, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    for (int phase = 0; phase < 3; ++phase) {
        BOOST_REQUIRE(sats[phase].size() == s[phase].size());
        for (size_t i = 0; i < s[phase].size(); ++i) {
            CHECK(sats[phase][i], s[phase][i], reltol);
        }
    }
}



BOOST_AUTO_TEST_CASE (DeckWithCapillaryOverlap)
{
    Opm::GridManager gm(1, 1, 20, 1.0, 1.0, 5.0);
    const UnstructuredGrid& grid = *(gm.c_grid());
    Opm::Parser parser;
    Opm::ParseContext parseContext;
    Opm::Deck deck = parser.parseFile("capillary_overlap.DATA" , parseContext);
    Opm::EclipseState eclipseState(deck , parseContext);
    Opm::BlackoilPropertiesFromDeck props(deck, eclipseState, grid, false);

    Opm::EQUIL::DeckDependent::InitialStateComputer comp(props, deck, eclipseState, grid, 9.80665);
    const auto& pressures = comp.press();
    BOOST_REQUIRE(pressures.size() == 3);
    BOOST_REQUIRE(int(pressures[0].size()) == grid.number_of_cells);

    const int first = 0, last = grid.number_of_cells - 1;
    // The relative tolerance is too loose to be very useful,
    // but the answer we are checking is the result of an ODE
    // solver, and it is unclear if we should check it against
    // the true answer or something else.
    const double reltol = 1.0e-6;
    const double reltol_ecl = 1.0;
    BOOST_CHECK_CLOSE(pressures[0][first], 1.48324e+07, reltol_ecl);  // eclipse 
    BOOST_CHECK_CLOSE(pressures[0][last],  1.54801e+07, reltol_ecl);
    BOOST_CHECK_CLOSE(pressures[1][first], 1.49224e+07, reltol_ecl);
    BOOST_CHECK_CLOSE(pressures[1][last],  1.54901e+07, reltol_ecl);
    
    BOOST_CHECK_CLOSE(pressures[0][first] , 14832467.14, reltol); // opm
    BOOST_CHECK_CLOSE(pressures[0][last ] , 15479883.47, reltol);
    BOOST_CHECK_CLOSE(pressures[1][last ] , 15489883.47, reltol);

    const auto& sats = comp.saturation();
    // std::cout << "Saturations:\n";
    // for (const auto& sat : sats) {
    //     for (const double s : sat) {
    //         std::cout << s << ' ';
    //     }
    //     std::cout << std::endl;
    // }
   
    const std::vector<double> s_ecl[3]{// eclipse
        { 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.22874042, 0.53397995, 0.78454906,  0.91542006, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0,   0,   0,   0,   0,   0,   0,   0,          0,          0.20039,     0.08458,    0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.77125955, 0.46602005, 0.015063271, 0,          0, 0, 0, 0, 0, 0, 0, 0, 0 }
    };

    const std::vector<double> s_opm[3]{ // opm 
        { 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2289309090909091,  0.53406545454545451, 0.78458,              0.9154, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0,   0,   0,   0,   0,   0,   0,   0,                   0,                   0.2002466666666666,   0.0846, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.77106909090909093, 0.46593454545454549, 0.015173333333333336, 0,      0, 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    for (int phase = 0; phase < 3; ++phase) {
        BOOST_REQUIRE(sats[phase].size() == s_opm[phase].size());
        for (size_t i = 0; i < s_opm[phase].size(); ++i) {
            //std::cout << std::setprecision(10) << sats[phase][i] << '\n';
            CHECK(sats[phase][i], s_ecl[phase][i], reltol_ecl);
            CHECK(sats[phase][i], s_opm[phase][i], reltol);
        }
    }
}



BOOST_AUTO_TEST_CASE (DeckWithLiveOil)
{
    Opm::GridManager gm(1, 1, 20, 1.0, 1.0, 5.0);
    const UnstructuredGrid& grid = *(gm.c_grid());
    Opm::Parser parser;
    Opm::ParseContext parseContext;
    Opm::Deck deck = parser.parseFile("equil_liveoil.DATA" , parseContext);
    Opm::EclipseState eclipseState(deck , parseContext);
    Opm::BlackoilPropertiesFromDeck props(deck, eclipseState, grid, false);

    Opm::EQUIL::DeckDependent::InitialStateComputer comp(props, deck, eclipseState, grid, 9.80665);
    const auto& pressures = comp.press();
    BOOST_REQUIRE(pressures.size() == 3);
    BOOST_REQUIRE(int(pressures[0].size()) == grid.number_of_cells);

    const int first = 0, last = grid.number_of_cells - 1;
    // The relative tolerance is too loose to be very useful,
    // but the answer we are checking is the result of an ODE
    // solver, and it is unclear if we should check it against
    // the true answer or something else.
    const double reltol = 1.0e-6;
    const double reltol_ecl = 1.0;
    BOOST_CHECK_CLOSE(pressures[0][first], 1.48324e+07, reltol_ecl);  // eclipse
    BOOST_CHECK_CLOSE(pressures[0][last],  1.54801e+07, reltol_ecl);
    BOOST_CHECK_CLOSE(pressures[1][first], 1.49224e+07, reltol_ecl);
    BOOST_CHECK_CLOSE(pressures[1][last],  1.54901e+07, reltol_ecl);
    
    BOOST_CHECK_CLOSE(pressures[0][first], 1.483246714e7, reltol);  // opm
    BOOST_CHECK_CLOSE(pressures[0][last],  1.547991652e7, reltol);
    BOOST_CHECK_CLOSE(pressures[1][first], 1.492246714e7, reltol);
    BOOST_CHECK_CLOSE(pressures[1][last],  1.548991652e7, reltol);

    const auto& sats = comp.saturation();
    // std::cout << "Saturations:\n";
    // for (const auto& sat : sats) {
    //     for (const double s : sat) {
    //         std::cout << s << ' ';
    //     }
    //     std::cout << std::endl;
    // }
    const std::vector<double> s_ecl[3]{ // eclipse
        { 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.22898, 0.53422, 0.78470, 0.91531, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0,   0,   0,   0,   0,   0,   0,   0,       0,       0.20073, 0.08469, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.77102, 0.46578, 0.01458, 0,       0, 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    const std::vector<double> s_opm[3]{ // opm 
        { 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2291709091, 0.5343054545, 0.78472,        0.91529, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0,   0,   0,   0,   0,   0,   0,   0,            0,            0.2005866667,   0.08471, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.7708290909, 0.4656945455, 0.01469333333,  0,       0, 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    for (int phase = 0; phase < 3; ++phase) {
        BOOST_REQUIRE(sats[phase].size() == s_opm[phase].size());
        for (size_t i = 0; i < s_opm[phase].size(); ++i) {
            //std::cout << std::setprecision(10) << sats[phase][i] << '\n';
            CHECK(sats[phase][i], s_opm[phase][i], reltol);
            CHECK(sats[phase][i], s_ecl[phase][i], reltol_ecl);
        }
        std::cout << std::endl;
    }
    
    const auto& rs = comp.rs();
    const std::vector<double> rs_opm {74.61233568, 74.64905212, 74.68578656, 74.72253902, // opm
                                      74.75930951, 74.79609803, 74.83290459, 74.87519876,
                                      74.96925416, 75.09067512, 75.0,        75.0, 
                                      75.0,        75.0,        75.0,        75.0, 
                                      75.0,        75.0,        75.0,        75.0};
    const std::vector<double> rs_ecl {74.612228, 74.648956, 74.685707, 74.722473,  // eclipse
                                      74.759254, 74.796051, 74.832870, 74.875145,
                                      74.969231, 75.090706, 75.000000, 75.000000,
                                      75.000000, 75.000000, 75.000000, 75.000000,
                                      75.000000, 75.000000, 75.000000, 75.000000}; 
    for (size_t i = 0; i < rs_opm.size(); ++i) {
        //std::cout << std::setprecision(10) << rs[i] << '\n';
        BOOST_CHECK_CLOSE(rs[i], rs_opm[i], reltol);
        BOOST_CHECK_CLOSE(rs[i], rs_ecl[i], reltol_ecl);
    }
}



BOOST_AUTO_TEST_CASE (DeckWithLiveGas)
{
    Opm::GridManager gm(1, 1, 20, 1.0, 1.0, 5.0);
    const UnstructuredGrid& grid = *(gm.c_grid());
    Opm::Parser parser;
    Opm::ParseContext parseContext;
    Opm::Deck deck = parser.parseFile("equil_livegas.DATA" , parseContext);
    Opm::EclipseState eclipseState(deck , parseContext);
    Opm::BlackoilPropertiesFromDeck props(deck, eclipseState, grid, false);

    Opm::EQUIL::DeckDependent::InitialStateComputer comp(props, deck, eclipseState, grid, 9.80665);
    const auto& pressures = comp.press();
    BOOST_REQUIRE(pressures.size() == 3);
    BOOST_REQUIRE(int(pressures[0].size()) == grid.number_of_cells);

    const int first = 0, last = grid.number_of_cells - 1;
    // The relative tolerance is too loose to be very useful,
    // but the answer we are checking is the result of an ODE
    // solver, and it is unclear if we should check it against
    // the true answer or something else.
    const double reltol = 5.0e-3;
    const double reltol_ecl = 1.0;
    BOOST_CHECK_CLOSE(pressures[0][first], 1.48215e+07, reltol_ecl);  // eclipse
    BOOST_CHECK_CLOSE(pressures[0][last],  1.54801e+07, reltol_ecl);
    BOOST_CHECK_CLOSE(pressures[1][first], 1.49115e+07, reltol_ecl);
    BOOST_CHECK_CLOSE(pressures[1][last],  1.54901e+07, reltol_ecl);
    
    BOOST_CHECK_CLOSE(pressures[0][first], 1.482150311e7, reltol);  // opm
    BOOST_CHECK_CLOSE(pressures[0][last],  1.547988347e7, reltol);
    BOOST_CHECK_CLOSE(pressures[1][first], 1.491150311e7, reltol);
    BOOST_CHECK_CLOSE(pressures[1][last],  1.548988347e7, reltol);

    const auto& sats = comp.saturation();
    // std::cout << "Saturations:\n";
    // for (const auto& sat : sats) {
    //     for (const double s : sat) {
    //         std::cout << s << ' ';
    //     }
    //     std::cout << std::endl;
    // }
    const std::vector<double> s_ecl[3]{ // eclipse
        { 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.24285614, 0.53869015, 0.78454906,  0.91542006, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0,   0,   0,   0,   0,   0,   0,   0,          0,          0.18311,     0.08458,    0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.75714386, 0.46130988, 0.032345835, 0,          0, 0, 0, 0, 0, 0, 0, 0, 0 }
    };

    const std::vector<double> s_opm[3]{ // opm 
        { 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.24310545, 0.5388, 0.78458,    0.91540, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0,   0,   0,   0,   0,   0,   0,   0,          0,      0.18288667, 0.0846,  0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.75689455, 0.4612, 0.03253333, 0,       0, 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    for (int phase = 0; phase < 3; ++phase) {
        BOOST_REQUIRE(sats[phase].size() == s_opm[phase].size());
        for (size_t i = 0; i < s_opm[phase].size(); ++i) {
            //std::cout << std::setprecision(10) << sats[phase][i] << '\n';
            CHECK(sats[phase][i], s_opm[phase][i], 100.*reltol);
            CHECK(sats[phase][i], s_ecl[phase][i], reltol_ecl);
        }
        std::cout << std::endl;
    }
    
    const auto& rv = comp.rv();
    const std::vector<double> rv_opm { // opm
        2.4884509e-4, 2.4910378e-4, 2.4936267e-4, 2.4962174e-4,
        2.4988100e-4, 2.5014044e-4, 2.5040008e-4, 2.5065990e-4, 
        2.5091992e-4, 2.5118012e-4, 2.5223082e-4, 2.5105e-4, 
        2.5105e-4,    2.5105e-4,    2.5105e-4,    2.5105e-4, 
        2.5105e-4,    2.5105e-4,    2.5105e-4,    2.5105e-4};

    const std::vector<double> rv_ecl {  // eclipse
        0.24884584E-03,   0.24910446E-03,   0.24936325E-03,   0.24962222E-03,
        0.24988138E-03,   0.25014076E-03,   0.25040031E-03,   0.25066003E-03,
        0.25091995E-03,   0.25118008E-03,   0.25223137E-03,   0.25104999E-03,
        0.25104999E-03,   0.25104999E-03,   0.25104999E-03,   0.25104999E-03,
        0.25104999E-03,   0.25104999E-03,   0.25104999E-03,   0.25104999E-03};
         
    for (size_t i = 0; i < rv_opm.size(); ++i) {
        //std::cout << std::setprecision(10) << sats[phase][i] << '\n';
        CHECK(rv[i], rv_opm[i], 100.*reltol);
        CHECK(rv[i], rv_ecl[i], reltol_ecl);
    }
}

BOOST_AUTO_TEST_CASE (DeckWithRSVDAndRVVD)
{
    Opm::GridManager gm(1, 1, 20, 1.0, 1.0, 5.0);
    const UnstructuredGrid& grid = *(gm.c_grid());
    Opm::ParseContext parseContext;
    Opm::Parser parser;
    Opm::Deck deck = parser.parseFile("equil_rsvd_and_rvvd.DATA", parseContext);
    Opm::EclipseState eclipseState(deck , parseContext);
    Opm::BlackoilPropertiesFromDeck props(deck, eclipseState, grid, false);

    Opm::EQUIL::DeckDependent::InitialStateComputer comp(props, deck, eclipseState, grid, 9.80665);
    const auto& pressures = comp.press();
    BOOST_REQUIRE(pressures.size() == 3);
    BOOST_REQUIRE(int(pressures[0].size()) == grid.number_of_cells);

    const int first = 0, last = grid.number_of_cells - 1;
    // The relative tolerance is too loose to be very useful,
    // but the answer we are checking is the result of an ODE
    // solver, and it is unclear if we should check it against
    // the true answer or something else.
    const double reltol = 1.0e-6;
    const double reltol_ecl = 1.0;
    BOOST_CHECK_CLOSE(pressures[0][first], 1.48350e+07, reltol_ecl);  // eclipse
    BOOST_CHECK_CLOSE(pressures[0][last],  1.54794e+07, reltol_ecl);
    BOOST_CHECK_CLOSE(pressures[1][first], 1.49250e+07, reltol_ecl);
    BOOST_CHECK_CLOSE(pressures[1][last],  1.54894e+07, reltol_ecl);
    
    BOOST_CHECK_CLOSE(pressures[0][first], 1.483499660e7, reltol);  // opm
    BOOST_CHECK_CLOSE(pressures[0][last],  1.547924516e7, reltol);
    BOOST_CHECK_CLOSE(pressures[1][first], 1.492499660e7, reltol);
    BOOST_CHECK_CLOSE(pressures[1][last],  1.548924516e7, reltol);

    const auto& sats = comp.saturation();
    // std::cout << "Saturations:\n";
    // for (const auto& sat : sats) {
    //     for (const double s : sat) {
    //         std::cout << s << ' ';
    //     }
    //     std::cout << std::endl;
    // }
    const std::vector<double> s_ecl[3]{ // eclipse
        { 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.22206347, 0.52871972, 0.78150368,  0.91819441,  1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0,   0,   0,   0,   0,   0,   0,   0,          0,          0.19656529,  0.081805572, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.77793652, 0.47128031, 0.021931054, 0,           0, 0, 0, 0, 0, 0, 0, 0, 0 }
    };

    const std::vector<double> s_opm[3]{ // opm 
        { 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.22232000, 0.52882909, 0.78153000,    0.91817000, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0,   0,   0,   0,   0,   0,   0,   0,          0,          0.19636333,    0.08183000, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.77768000, 0.47117091, 0.02210667,    0,          0, 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    
    for (int phase = 0; phase < 3; ++phase) {
        BOOST_REQUIRE(sats[phase].size() == s_opm[phase].size());
        for (size_t i = 0; i < s_opm[phase].size(); ++i) {
            //std::cout << std::setprecision(10) << sats[phase][i] << '\n';
            CHECK(sats[phase][i], s_opm[phase][i], 100.*reltol);
            CHECK(sats[phase][i], s_ecl[phase][i], reltol_ecl);
        }
        std::cout << std::endl;
    }
    
    const auto& rs = comp.rs();
    const std::vector<double> rs_opm { // opm
        74.62498302, 74.65959041, 74.69438035, 74.72935336,
        74.76450995, 74.79985061, 74.83537588, 74.87527125,
        74.96863769, 75.08891765, 52.5,        57.5,
        62.5,        67.5,        72.5,        76.45954841,
        76.70621045, 76.95287736, 77.19954913, 77.44622578};

    const std::vector<double> rs_ecl {  // eclipse
        74.625114, 74.659706, 74.694481, 74.729439,
        74.764580, 74.799904, 74.835419, 74.875252,
        74.968628, 75.088951, 52.500000, 57.500000,
        62.500000, 67.500000, 72.500000, 76.168388,
        76.349953, 76.531532, 76.713142, 76.894775,};
    
    const auto& rv = comp.rv();
    const std::vector<double> rv_opm { // opm
        2.50e-6, 7.50e-6,       1.25e-5,       1.75e-5,
        2.25e-5, 2.75e-5,       3.25e-5,       3.75e-5, 
        4.25e-5, 2.51158386e-4, 2.52203372e-4, 5.75e-5, 
        6.25e-5, 6.75e-5,       7.25e-5,       7.75e-5, 
        8.25e-5, 8.75e-5,       9.25e-5,       9.75e-5};

    const std::vector<double> rv_ecl {  // eclipse
        0.24999999E-05, 0.74999998E-05, 0.12500000E-04, 0.17500000E-04,
        0.22500000E-04, 0.27500000E-04, 0.32500000E-04, 0.37500002E-04,
        0.42500000E-04, 0.25115837E-03, 0.25220393E-03, 0.57500001E-04,
        0.62500003E-04, 0.67499997E-04, 0.72499999E-04, 0.77500001E-04,
        0.82500002E-04, 0.87499997E-04, 0.92499999E-04, 0.97500000E-04};
         
    for (size_t i = 0; i < rv_opm.size(); ++i) {
        //std::cout << std::setprecision(10) << rs[i] << '\n';
        BOOST_CHECK_CLOSE(rs[i], rs_opm[i], 100*reltol);
        BOOST_CHECK_CLOSE(rs[i], rs_ecl[i], reltol_ecl);
        BOOST_CHECK_CLOSE(rv[i], rv_opm[i], 100.*reltol);
        BOOST_CHECK_CLOSE(rv[i], rv_ecl[i], reltol_ecl);
    }
}

BOOST_AUTO_TEST_CASE (DeckWithSwatinit)
{
    //Opm::GridManager gm(1, 1, 20, 1.0, 1.0, 5.0);
    Opm::Parser parser;
    Opm::ParseContext parseContext;
    Opm::Deck deck = parser.parseFile("capillarySwatinit.DATA" , parseContext);
    Opm::EclipseState eclipseState(deck , parseContext);
    Opm::GridManager gm(eclipseState.getInputGrid());
    const UnstructuredGrid& grid = *(gm.c_grid());
    Opm::BlackoilPropertiesFromDeck props(deck, eclipseState, grid, false);
    Opm::BlackoilPropertiesFromDeck propsScaled(deck, eclipseState, grid, false);

    Opm::BlackoilState state( Opm::UgGridHelpers::numCells( grid ) , Opm::UgGridHelpers::numFaces( grid ) , 3);

    // reference saturations
    const std::vector<double> s[3]{
        { 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.425307, 0.77464, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0, 0, 0, 0.0148267, 0.78528, 0.8, 0.8, 0.8, 0.8, 0.574693, 0.22536, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0.8, 0.8, 0.8, 0.785173, 0.01472, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    // sw in cell 13 and 14 is forced to be swu=1 since P_oil - P_wat < 0.
    const std::vector<double> swatinit[3]{
        { 0.2, 0.2, 0.2, 0.2, 0.2, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0, 0, 0, 0.0148267, 0.78528, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0.8, 0.8, 0.8, 0.785173, 0.01472, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    std::vector<double> sats = state.saturation();
    for (int phase = 0; phase < 3; ++phase) {
        for (size_t i = 0; i < 20; ++i) {
            sats[3*i + phase] = s[phase][i];
        }
    }
    std::vector<double> sats_swatinit = state.saturation();
    for (int phase = 0; phase < 3; ++phase) {
        for (size_t i = 0; i < 20; ++i) {
            sats_swatinit[3*i + phase] = swatinit[phase][i];
        }
    }

    // reference pcs
    const int numCells = Opm::UgGridHelpers::numCells(grid);
    std::vector<int> cells(numCells);
    for (int c = 0; c < numCells; ++c) { cells[c] = c; }
    std::vector<double> pc_original = state.saturation();
    props.capPress(numCells, sats.data(), cells.data(), pc_original.data(), nullptr);

    std::vector<double> pc_scaled_truth = pc_original;

    // modify pcow for cell 5 - 11 (where sw is changed due to swatinit)
    // for the reference scaled pc.
    pc_scaled_truth[3*5 + 0] = 84081;
    pc_scaled_truth[3*6 + 0] = 70929;
    pc_scaled_truth[3*7 + 0] = 57791;
    pc_scaled_truth[3*8 + 0] = 44665;
    pc_scaled_truth[3*9 + 0] = 31552;
    pc_scaled_truth[3*10 + 0] = 18451.5;
    pc_scaled_truth[3*11 + 0] =  5364.1;


    // compute the initial state

    // apply swatinit
    Opm::BlackoilState state_scaled = state;
    initStateEquil(grid, propsScaled, deck, eclipseState, 9.81, state_scaled, true);

    // don't apply swatinit
    Opm::BlackoilState state_unscaled = state;
    initStateEquil(grid, props, deck, eclipseState, 9.81, state_unscaled, false);

    // compute pc
    std::vector<double> pc_scaled= state.saturation();
    propsScaled.capPress(numCells, state_scaled.saturation().data(), cells.data(), pc_scaled.data(), nullptr);
    std::vector<double> pc_unscaled= state.saturation();
    props.capPress(numCells, state_unscaled.saturation().data(), cells.data(), pc_unscaled.data(), nullptr);

    // test
    const double reltol = 1.0e-3;
    for (int phase = 0; phase < 3; ++phase) {
        for (size_t i = 0; i < 20; ++i) {
            CHECK( pc_original[3*i + phase ], pc_unscaled[3*i + phase ], reltol);
            CHECK( pc_scaled_truth[3*i + phase], pc_scaled[3*i + phase ], reltol);
        }
    }

    for (int phase = 0; phase < 3; ++phase) {
        for (size_t i = 0; i < 20; ++i) {
            CHECK(state_unscaled.saturation()[3*i + phase], s[phase][i], reltol);
            CHECK(state_scaled.saturation()[3*i + phase], swatinit[phase][i], reltol);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
