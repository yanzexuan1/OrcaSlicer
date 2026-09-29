#ifndef slic3r_GCode_IntamGCode_hpp_
#define slic3r_GCode_IntamGCode_hpp_

#include <string>

namespace Slic3r {

// Keep Orca body comments (;TYPE:Outer wall, ;LAYER_CHANGE, ...).
// Ensure Intam START/END header keys for IsIntamGCode, and ";End of Gcode" at the file end.
// Intam firmware does not support M73; comment those lines out during Intam export.
// Intam firmware only honors M204 S; rewrite Marlin 2 M204 P/T(/R) to M204 S.
bool convert_gcode_to_intam_dialect(const std::string &in_path, const std::string &out_path);

} // namespace Slic3r

#endif
