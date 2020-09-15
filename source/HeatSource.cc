/* Copyright (c) 2016 - 2020, the adamantine authors.
 *
 * This file is subject to the Modified BSD License and may not be distributed
 * without copyright and license information. Please refer to the file LICENSE
 * for the text and further information on this license.
 */

#ifndef HEAT_SOURCE_TEMPLATES_HH
#define HEAT_SOURCE_TEMPLATES_HH

#include <HeatSource.hh>
#include <instantiation.hh>
#include <utils.hh>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <cstdlib>

using std::pow;

namespace adamantine
{
/*
namespace internal
{
class PointSource : public dealii::Function<1>
{
public:
  PointSource(std::vector<double> const &position);

  double value(dealii::Point<1> const &time,
               unsigned int const component = 0) const override;

  void rewind_time();

  void save_time();

private:
  mutable unsigned int _current_pos;
  unsigned int _saved_pos;
  mutable dealii::Point<1> _current_time;
  dealii::Point<1> _saved_time;
  std::vector<double> _position;
};

PointSource::PointSource(std::vector<double> const &position)
    : _current_pos(-1), _saved_pos(-1), _position(position)
{
  _current_time[0] = -1.;
}

double PointSource::value(dealii::Point<1> const &time,
                          unsigned int const) const
{
  // If the time is greater than the current one, we use the next entry in the
  // vector.
  if (time[0] > _current_time[0])
  {
    ++_current_pos;
    _current_time[0] = time[0];
  }

  return _position[_current_pos];
}

void PointSource::rewind_time()
{
  _current_pos = _saved_pos;
  _current_time = _saved_time;
}

void PointSource::save_time()
{
  _saved_pos = _current_pos;
  _saved_time = _current_time;
}
} // namespace internal
*/

template <int dim>
HeatSource<dim>::HeatSource(boost::property_tree::ptree const &database)
    : dealii::Function<dim>(), _max_height(0.)
{
  // Set the properties of the electron beam.
  _beam.depth = database.get<double>("depth");
  _beam.energy_conversion_eff =
      database.get<double>("energy_conversion_efficiency");
  _beam.control_eff = database.get<double>("control_efficiency");
  _beam.diameter_squared = pow(database.get("diameter", 2e-3), 2);
  boost::optional<double> max_power =
      database.get_optional<double>("max_power");
  if (max_power)
    _beam.max_power = max_power.get();
  else
  {
    double const current = database.get<double>("current");
    double const voltage = database.get<double>("voltage");
    _beam.max_power = current * voltage;
  }

  // The only variable that can be used to define the position is the time t.
  std::string variable = "t";
  // Predefined constants
  std::map<std::string, double> constants;
  constants["pi"] = dealii::numbers::PI;

 // Parse the scan path
 // TODO

}

template <int dim>
std::vector<ScanPathSegment> HeatSource<dim>::parseScanPath(std::string scan_path_file)
{

  std::vector<ScanPathSegment> segments;

  // Open the file
  ASSERT_THROW(boost::filesystem::exists(scan_path_file),
               "The file " + scan_path_file + " does not exist.");
  std::ifstream file;
  file.open(scan_path_file);
  std::string line;
  int line_index = 0;
  while (getline(file, line))
  {
    std::cout << line << std::endl;

    // Skip the header
    if (line_index > 2)
    {
      std::vector<std::string> split_line;
      boost::split(split_line, line, boost::is_any_of(" "),
                   boost::token_compress_on);
      ScanPathSegment segment;

      // Set the segment type
      ScanPathSegmentType segment_type;
      if (split_line[0] == "0")
      {
        if (segments.size() == 0)
        {
          std::string message =
              "Error: Scan paths must begin with a 'point' segment.";
          throw std::runtime_error(message);
        }
        segment_type = ScanPathSegmentType::line;
      }
      else if (split_line[0] == "1")
      {
        segment_type = ScanPathSegmentType::point;
      }
      else
      {
        std::string message = "Error: Mode type in scan path file line " +
                              std::to_string(line_index) + "not recognized.";
        throw std::runtime_error(message);
      }

      // Set the segment end position
      segment.end_point(0) = std::stod(split_line[1]);
      segment.end_point(1) = std::stod(split_line[2]);
      segment.end_point(2) = std::stod(split_line[3]);

      // Set the power modifier
      segment.power_modifier = std::stod(split_line[4]);

      // Set the velocity and end time
      if (segment_type == ScanPathSegmentType::point)
      {
        if (segments.size() > 0)
        {
          segment.end_time =
              segments.back().end_time + std::stod(split_line[5]);
        }
        else
        {
          segment.end_time = std::stod(split_line[5]);
        }
      }
      else
      {
        double velocity = std::stod(split_line[5]);
        double line_length =
            segment.end_point.distance(segments.back().end_point);
        segment.end_time =
            segments.back().end_time + std::abs(line_length / velocity);
      }
      segments.push_back(segment);
    }
    line_index++;
  }
  file.close();

  return segments;
}

template <int dim>
void HeatSource<dim>::rewind_time()
{

}

template <int dim>
void HeatSource<dim>::save_time()
{

}

template <int dim>
double HeatSource<dim>::value(dealii::Point<dim> const &point,
                                unsigned int const /*component*/) const
{
    /*
  double const z = point[1] - _max_height;
  if ((z + _beam.depth) < 0.)
    return 0.;
  else
  {
    double const distribution_z =
        -3. * pow(z / _beam.depth, 2) - 2. * (z / _beam.depth) + 1.;

    dealii::Point<1> time;
    time[0] = this->get_time();

    double const beam_center_x = _position[0]->value(time);

    double xpy_squared = pow(point[0] - beam_center_x, 2);
    if (dim == 3)
    {
      double const beam_center_y = _position[1]->value(time);
      xpy_squared += pow(point[2] - beam_center_y, 2);
    }

    double heat_source = 0.;
    heat_source =
        -_beam.energy_conversion_eff * _beam.control_eff * _beam.max_power *
        (4. * std::log(0.1)) /
        (dealii::numbers::PI * _beam.diameter_squared * _beam.depth) *
        std::exp((4. * std::log(0.1)) * xpy_squared / _beam.diameter_squared) *
        distribution_z;

    return heat_source;

  }
  */
}
} // namespace adamantine

INSTANTIATE_DIM(HeatSource)

#endif
