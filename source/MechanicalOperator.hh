/* Copyright (c) 2022, the adamantine authors.
 *
 * This file is subject to the Modified BSD License and may not be distributed
 * without copyright and license information. Please refer to the file LICENSE
 * for the text and further information on this license.
 */

#ifndef MECHANICAL_OPERATOR_HH
#define MECHANICAL_OPERATOR_HH

#include <MaterialProperty.hh>
#include <Operator.hh>

#include <deal.II/base/memory_space.h>
#include <deal.II/base/quadrature_lib.h>
#include <deal.II/dofs/dof_handler.h>
#include <deal.II/hp/q_collection.h>
#include <deal.II/lac/affine_constraints.h>
#include <deal.II/lac/la_parallel_vector.h>
#include <deal.II/lac/trilinos_sparse_matrix.h>

#include <boost/property_tree/ptree.hpp>

namespace adamantine
{
/**
 * This class is the operator associated with the solid mechanics equations.
 * The class is templated on the MemorySpace because it use MaterialProperty
 * which itself is templated on the MemorySpace but the operator is CPU only.
 */
template <int dim, typename MemorySpaceType>
class MechanicalOperator : public Operator<dealii::MemorySpace::Host>
{
public:
  /**
   * Constructor. If the initial temperature is negative, the simulation is
   * mechanical only. Otherwise, we solve a thermo-mechanical problem.
   */
  MechanicalOperator(
      MPI_Comm const &communicator,
      MaterialProperty<dim, MemorySpaceType> &material_properties,
      std::vector<double> reference_temperatures, bool include_gravity = false);

  void reinit(dealii::DoFHandler<dim> const &dof_handler,
              dealii::AffineConstraints<double> const &affine_constraints,
              dealii::hp::QCollection<dim> const &quad);

  dealii::types::global_dof_index m() const override;

  dealii::types::global_dof_index n() const override;

  void
  vmult(dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> &dst,
        dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> const
            &src) const override;

  void Tvmult(
      dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> &dst,
      dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> const
          &src) const override;

  void vmult_add(
      dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> &dst,
      dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> const
          &src) const override;

  void Tvmult_add(
      dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> &dst,
      dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> const
          &src) const override;
  /**
   * Update the DoFHandler used by ThermalPhysics and update the temperature.
   */
  void update_temperature(
      dealii::DoFHandler<dim> const &thermal_dof_handler,
      dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> const
          &temperature,
      std::vector<double> const &has_melted_indicator);

  dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> const &
  rhs() const;

  dealii::TrilinosWrappers::SparseMatrix const &system_matrix() const;

private:
  /**
   * Assemble the matrix and the right-hand-side.
   * @Note The 2D case does not represent any physical model but it is
   * convenient for testing.
   */
  void assemble_system();

  /**
   * MPI communicator.
   */
  MPI_Comm const &_communicator;
  /**
   * Output the latex formula of the bilinear form
   */
  bool _bilinear_form_output = true;
  /**
   * Whether to include a gravitional body force in the calculation.
   */
  bool _include_gravity = false;
  /**
   * List of initial temperatures of the material. If the length of the vector
   * is nonzero, we solve a themo-mechanical problem. Otherwise, we solve a
   * mechanical only problem. The vector index refers to the user index for a
   * given cell (i.e. if the cell user index is "1", the appropriate reference
   * temperature is _reference_temperatures[1]).
   */
  std::vector<double> _reference_temperatures;
  /**
   * Reference to the MaterialProperty from MechanicalPhysics.
   */
  MaterialProperty<dim, MemorySpaceType> &_material_properties;
  /**
   * Non-owning pointer to the DoFHandler from MechanicalPhysics
   */
  dealii::DoFHandler<dim> const *_dof_handler = nullptr;
  /**
   * Non-owning pointer to the DoFHandler from ThermalPhysics
   */
  dealii::DoFHandler<dim> const *_thermal_dof_handler = nullptr;
  /**
   * Non-owning pointer to the AffineConstraints from MechanicalPhysics
   */
  dealii::AffineConstraints<double> const *_affine_constraints = nullptr;
  /**
   * Non-owning pointer to the QCollection from MechanicalPhysics
   */
  dealii::hp::QCollection<dim> const *_q_collection = nullptr;
  /**
   * Right-hand-side of the mechanical problem.
   */
  dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host>
      _system_rhs;
  /**
   * Matrix of the mechanical problem.
   */
  dealii::TrilinosWrappers::SparseMatrix _system_matrix;
  /**
   * Temperature of the material.
   */
  dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host>
      _temperature;
  /**
   * Indicator variable for whether a point has ever been above the solidus. The
   * value is 0 for material that has not yet melted and 1 for material that has
   * melted. Due to how the state is transferred for remeshing, this has to be
   * stored as a double.
   */
  std::vector<double> _has_melted_indicator;
};

template <int dim, typename MemorySpaceType>
inline dealii::types::global_dof_index
MechanicalOperator<dim, MemorySpaceType>::m() const
{
  return _system_matrix.m();
}

template <int dim, typename MemorySpaceType>
inline dealii::types::global_dof_index
MechanicalOperator<dim, MemorySpaceType>::n() const
{
  return _system_matrix.n();
}

template <int dim, typename MemorySpaceType>
inline dealii::LA::distributed::Vector<double,
                                       dealii::MemorySpace::Host> const &
MechanicalOperator<dim, MemorySpaceType>::rhs() const
{
  return _system_rhs;
}

template <int dim, typename MemorySpaceType>
inline dealii::TrilinosWrappers::SparseMatrix const &
MechanicalOperator<dim, MemorySpaceType>::system_matrix() const
{
  return _system_matrix;
}
} // namespace adamantine
#endif
