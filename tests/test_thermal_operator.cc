/* Copyright (c) 2016, the adamantine authors.
 *
 * This file is subject to the Modified BSD License and may not be distributed
 * without copyright and license information. Please refer to the file LICENSE
 * for the text and further information on this license.
 */

#define BOOST_TEST_MODULE ThermalOperator

#include "main.cc"

#include "Geometry.hh"
#include "ThermalOperator.hh"
#include <boost/mpi.hpp>
#include <boost/property_tree/ptree.hpp>

BOOST_AUTO_TEST_CASE(thermal_operator)
{
  boost::mpi::communicator communicator;
  boost::property_tree::ptree mat_prop_database;
  mat_prop_database.put("n_materials", 1);
  mat_prop_database.put("material_0.solid.density", 1.);
  mat_prop_database.put("material_0.powder.density", 1.);
  mat_prop_database.put("material_0.liquid.density", 1.);
  mat_prop_database.put("material_0.solid.specific_heat", 1.);
  mat_prop_database.put("material_0.powder.specific_heat", 1.);
  mat_prop_database.put("material_0.liquid.specific_heat", 1.);
  mat_prop_database.put("material_0.solid.thermal_conductivity", 10.);
  mat_prop_database.put("material_0.powder.thermal_conductivity", 10.);
  mat_prop_database.put("material_0.liquid.thermal_conductivity", 10.);
  std::shared_ptr<adamantine::MaterialProperty> mat_properties(
      new adamantine::MaterialProperty(mat_prop_database));

  // Create the Geometry
  boost::property_tree::ptree geometry_database;
  geometry_database.put("length", 12);
  geometry_database.put("length_divisions", 4);
  geometry_database.put("height", 6);
  geometry_database.put("height_divisions", 5);
  adamantine::Geometry<2> geometry(communicator, geometry_database);
  // Create the DoFHandler
  dealii::FE_Q<2> fe(2);
  dealii::DoFHandler<2> dof_handler(geometry.get_triangulation());
  dof_handler.distribute_dofs(fe);
  dealii::ConstraintMatrix constraint_matrix;
  constraint_matrix.close();
  dealii::QGauss<1> quad(3);

  // Initialize the ThermalOperator
  adamantine::ThermalOperator<2, 2, double> thermal_operator(communicator,
                                                             mat_properties);
  thermal_operator.reinit(dof_handler, constraint_matrix, quad);
  BOOST_CHECK(thermal_operator.m() == 99);
  BOOST_CHECK(thermal_operator.m() == thermal_operator.n());

  // Check matrix-vector multiplications
  double const tolerance = 1e-15;
  dealii::LA::distributed::Vector<double> src;
  dealii::LA::distributed::Vector<double> dst_1;
  dealii::LA::distributed::Vector<double> dst_2;

  dealii::MatrixFree<2, double> const &matrix_free =
      thermal_operator.get_matrix_free();
  matrix_free.initialize_dof_vector(src);
  matrix_free.initialize_dof_vector(dst_1);
  matrix_free.initialize_dof_vector(dst_2);

  src = 1.;
  thermal_operator.vmult(dst_1, src);
  BOOST_CHECK_CLOSE(dst_1.l1_norm(), 0., tolerance);

  thermal_operator.Tvmult(dst_2, src);
  BOOST_CHECK_CLOSE(dst_2.l1_norm(), dst_1.l1_norm(), tolerance);

  dst_2 = 1.;
  thermal_operator.vmult_add(dst_2, src);
  thermal_operator.vmult(dst_1, src);
  dst_1 += src;
  BOOST_CHECK_CLOSE(dst_1.l1_norm(), dst_2.l1_norm(), tolerance);

  dst_1 = 1.;
  thermal_operator.Tvmult_add(dst_1, src);
  BOOST_CHECK_CLOSE(dst_1.l1_norm(), dst_2.l1_norm(), tolerance);
}
