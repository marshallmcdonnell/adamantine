/* Copyright (c) 2016 - 2021, the adamantine authors.
 *
 * This file is subject to the Modified BSD License and may not be distributed
 * without copyright and license information. Please refer to the file LICENSE
 * for the text and further information on this license.
 */

#define BOOST_TEST_MODULE PostProcessor

#include <Geometry.hh>
#include <GoldakHeatSource.hh>
#include <MaterialProperty.hh>
#include <MechanicalOperator.hh>
#include <PostProcessor.hh>
#include <ThermalOperator.hh>

#include <deal.II/dofs/dof_tools.h>
#include <deal.II/fe/fe_nothing.h>
#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_system.h>
#include <deal.II/numerics/vector_tools.h>

#include <boost/filesystem.hpp>

#include "main.cc"

BOOST_AUTO_TEST_CASE(thermal_post_processor)
{
  MPI_Comm communicator = MPI_COMM_WORLD;

  // Create the Geometry
  boost::property_tree::ptree geometry_database;
  geometry_database.put("import_mesh", false);
  geometry_database.put("length", 12);
  geometry_database.put("length_divisions", 4);
  geometry_database.put("height", 6);
  geometry_database.put("height_divisions", 5);
  adamantine::Geometry<2> geometry(communicator, geometry_database);
  // Create the DoFHandler
  dealii::hp::FECollection<2> fe_collection;
  fe_collection.push_back(dealii::FE_Q<2>(2));
  fe_collection.push_back(dealii::FE_Nothing<2>());
  dealii::DoFHandler<2> dof_handler(geometry.get_triangulation());
  dof_handler.distribute_dofs(fe_collection);
  dealii::AffineConstraints<double> affine_constraints;
  affine_constraints.close();
  dealii::hp::QCollection<1> q_collection;
  q_collection.push_back(dealii::QGauss<1>(3));
  q_collection.push_back(dealii::QGauss<1>(1));

  // Create the MaterialProperty
  boost::property_tree::ptree mat_prop_database;
  mat_prop_database.put("property_format", "polynomial");
  mat_prop_database.put("n_materials", 1);
  mat_prop_database.put("material_0.solid.density", 1.);
  mat_prop_database.put("material_0.powder.density", 1.);
  mat_prop_database.put("material_0.liquid.density", 1.);
  mat_prop_database.put("material_0.solid.specific_heat", 1.);
  mat_prop_database.put("material_0.powder.specific_heat", 1.);
  mat_prop_database.put("material_0.liquid.specific_heat", 1.);
  mat_prop_database.put("material_0.solid.thermal_conductivity_x", 10.);
  mat_prop_database.put("material_0.solid.thermal_conductivity_z", 10.);
  mat_prop_database.put("material_0.powder.thermal_conductivity_x", 10.);
  mat_prop_database.put("material_0.powder.thermal_conductivity_z", 10.);
  mat_prop_database.put("material_0.liquid.thermal_conductivity_x", 10.);
  mat_prop_database.put("material_0.liquid.thermal_conductivity_z", 10.);
  std::shared_ptr<adamantine::MaterialProperty<2, dealii::MemorySpace::Host>>
      mat_properties(
          new adamantine::MaterialProperty<2, dealii::MemorySpace::Host>(
              communicator, geometry.get_triangulation(), mat_prop_database));

  boost::property_tree::ptree beam_database;
  beam_database.put("depth", 0.1);
  beam_database.put("absorption_efficiency", 0.1);
  beam_database.put("diameter", 1.0);
  beam_database.put("max_power", 10.);
  beam_database.put("scan_path_file", "scan_path.txt");
  beam_database.put("scan_path_file_format", "segment");
  std::vector<std::shared_ptr<adamantine::HeatSource<2>>> heat_sources;
  heat_sources.resize(1);
  heat_sources[0] =
      std::make_shared<adamantine::GoldakHeatSource<2>>(beam_database);

  // Initialize the ThermalOperator
  adamantine::ThermalOperator<2, 2, dealii::MemorySpace::Host> thermal_operator(
      communicator, adamantine::BoundaryType::adiabatic, mat_properties,
      heat_sources);
  std::vector<double> deposition_cos(
      geometry.get_triangulation().n_locally_owned_active_cells(), 1.);
  std::vector<double> deposition_sin(
      geometry.get_triangulation().n_locally_owned_active_cells(), 0.);
  thermal_operator.reinit(dof_handler, affine_constraints, q_collection);
  thermal_operator.set_material_deposition_orientation(deposition_cos,
                                                       deposition_sin);
  thermal_operator.compute_inverse_mass_matrix(dof_handler, affine_constraints,
                                               fe_collection);

  // Create the PostProcessor
  boost::property_tree::ptree post_processor_database;
  post_processor_database.put("filename_prefix", "test");
  post_processor_database.put("thermal_output", true);
  adamantine::PostProcessor<2> post_processor(
      communicator, post_processor_database, dof_handler);
  dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> src;
  dealii::MatrixFree<2, double> const &matrix_free =
      thermal_operator.get_matrix_free();
  matrix_free.initialize_dof_vector(src);
  for (unsigned int i = 0; i < src.size(); ++i)
    src[i] = 1.;

  post_processor.write_thermal_output(
      1, 0, 0., src, mat_properties->get_state(),
      mat_properties->get_dofs_map(), mat_properties->get_dof_handler());
  post_processor.write_thermal_output(
      1, 1, 0.1, src, mat_properties->get_state(),
      mat_properties->get_dofs_map(), mat_properties->get_dof_handler());
  post_processor.write_thermal_output(
      1, 2, 0.2, src, mat_properties->get_state(),
      mat_properties->get_dofs_map(), mat_properties->get_dof_handler());
  post_processor.write_pvd();

  // Check that the files exist
  BOOST_CHECK(boost::filesystem::exists("test.pvd"));
  BOOST_CHECK(boost::filesystem::exists("test.01.000000.pvtu"));
  BOOST_CHECK(boost::filesystem::exists("test.01.000001.pvtu"));
  BOOST_CHECK(boost::filesystem::exists("test.01.000002.pvtu"));
  BOOST_CHECK(boost::filesystem::exists("test.01.000000.000000.vtu"));
  BOOST_CHECK(boost::filesystem::exists("test.01.000001.000000.vtu"));
  BOOST_CHECK(boost::filesystem::exists("test.01.000002.000000.vtu"));

  // Delete the files
  std::remove("test.pvd");
  std::remove("test.01.000000.pvtu");
  std::remove("test.01.000001.pvtu");
  std::remove("test.01.000002.pvtu");
  std::remove("test.01.000000.000000.vtu");
  std::remove("test.01.000001.000000.vtu");
  std::remove("test.01.000002.000000.vtu");
}

#ifdef ADAMANTINE_WITH_DEALII_WEAK_FORMS
BOOST_AUTO_TEST_CASE(mechanical_post_processor)
{
  MPI_Comm communicator = MPI_COMM_WORLD;
  int constexpr dim = 3;

  // Create the Geometry
  boost::property_tree::ptree geometry_database;
  geometry_database.put("import_mesh", false);
  geometry_database.put("length", 6);
  geometry_database.put("length_divisions", 3);
  geometry_database.put("height", 6);
  geometry_database.put("height_divisions", 3);
  geometry_database.put("width", 6);
  geometry_database.put("width_divisions", 3);
  adamantine::Geometry<dim> geometry(communicator, geometry_database);
  // Create the DoFHandler
  dealii::hp::FECollection<dim> fe_collection;
  fe_collection.push_back(dealii::FESystem<dim>(dealii::FE_Q<dim>(2) ^ dim));
  fe_collection.push_back(
      dealii::FESystem<dim>(dealii::FE_Nothing<dim>() ^ dim));
  dealii::DoFHandler<dim> dof_handler(geometry.get_triangulation());
  dof_handler.distribute_dofs(fe_collection);
  dealii::AffineConstraints<double> affine_constraints;
  dealii::DoFTools::make_hanging_node_constraints(dof_handler,
                                                  affine_constraints);
  dealii::VectorTools::interpolate_boundary_values(
      dof_handler, 0, dealii::Functions::ZeroFunction<dim>(dim),
      affine_constraints);
  affine_constraints.close();
  dealii::hp::QCollection<dim> q_collection;
  q_collection.push_back(dealii::QGauss<dim>(3));
  q_collection.push_back(dealii::QGauss<dim>(1));
  // Create the MaterialProperty
  boost::property_tree::ptree mat_prop_database;
  mat_prop_database.put("property_format", "polynomial");
  mat_prop_database.put("n_materials", 1);
  mat_prop_database.put("material_0.solid.density", 1.);
  mat_prop_database.put("material_0.powder.density", 1.);
  mat_prop_database.put("material_0.liquid.density", 1.);
  mat_prop_database.put("material_0.solid.specific_heat", 1.);
  mat_prop_database.put("material_0.powder.specific_heat", 1.);
  mat_prop_database.put("material_0.liquid.specific_heat", 1.);
  mat_prop_database.put("material_0.solid.thermal_conductivity_x", 10.);
  mat_prop_database.put("material_0.solid.thermal_conductivity_z", 10.);
  mat_prop_database.put("material_0.powder.thermal_conductivity_x", 10.);
  mat_prop_database.put("material_0.powder.thermal_conductivity_z", 10.);
  mat_prop_database.put("material_0.liquid.thermal_conductivity_x", 10.);
  mat_prop_database.put("material_0.liquid.thermal_conductivity_z", 10.);
  std::shared_ptr<adamantine::MaterialProperty<dim, dealii::MemorySpace::Host>>
      mat_properties(
          new adamantine::MaterialProperty<dim, dealii::MemorySpace::Host>(
              communicator, geometry.get_triangulation(), mat_prop_database));
  // Create the mechanical database
  boost::property_tree::ptree mechanical_database;
  double const lame_first = 2.;
  double const lame_second = 3.;
  mechanical_database.put("lame_first_param", lame_first);
  mechanical_database.put("lame_second_param", lame_second);

  adamantine::MechanicalOperator<dim> mechanical_operator(communicator,
                                                          mechanical_database);
  mechanical_operator.reinit(dof_handler, affine_constraints, q_collection);

  // Create the PostProcessor
  boost::property_tree::ptree post_processor_database;
  post_processor_database.put("filename_prefix", "test");
  post_processor_database.put("mechanical_output", true);
  adamantine::PostProcessor<dim> post_processor(
      communicator, post_processor_database, dof_handler);
  dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> src(
      dof_handler.n_dofs());
  for (unsigned int i = 0; i < src.size(); ++i)
    src[i] = i % (dim + 1);

  post_processor.write_mechanical_output(
      1, 0, 0., src, mat_properties->get_state(),
      mat_properties->get_dofs_map(), mat_properties->get_dof_handler());
  post_processor.write_mechanical_output(
      1, 1, 0.1, src, mat_properties->get_state(),
      mat_properties->get_dofs_map(), mat_properties->get_dof_handler());
  post_processor.write_mechanical_output(
      1, 2, 0.2, src, mat_properties->get_state(),
      mat_properties->get_dofs_map(), mat_properties->get_dof_handler());
  post_processor.write_pvd();

  // Check that the files exist
  BOOST_TEST(boost::filesystem::exists("test.pvd"));
  BOOST_TEST(boost::filesystem::exists("test.01.000000.pvtu"));
  BOOST_TEST(boost::filesystem::exists("test.01.000001.pvtu"));
  BOOST_TEST(boost::filesystem::exists("test.01.000002.pvtu"));
  BOOST_TEST(boost::filesystem::exists("test.01.000000.000000.vtu"));
  BOOST_TEST(boost::filesystem::exists("test.01.000001.000000.vtu"));
  BOOST_TEST(boost::filesystem::exists("test.01.000002.000000.vtu"));

  // Delete the files
  std::remove("test.pvd");
  std::remove("test.01.000000.pvtu");
  std::remove("test.01.000001.pvtu");
  std::remove("test.01.000002.pvtu");
  std::remove("test.01.000000.000000.vtu");
  std::remove("test.01.000001.000000.vtu");
  std::remove("test.01.000002.000000.vtu");
}
#endif
