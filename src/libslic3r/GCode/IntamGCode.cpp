#include "IntamGCode.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/nowide/fstream.hpp>
#include <cctype>
#include <sstream>

namespace Slic3r {

namespace {

std::string trim_copy(std::string s)
{
    boost::trim(s);
    if (!s.empty() && s.back() == '\r')
        s.pop_back();
    boost::trim(s);
    return s;
}

std::string to_lower(std::string s)
{
    boost::algorithm::to_lower(s);
    return s;
}

constexpr const char *kIntamLineTypeTimes[] = {
    "PRINT_TIME.LINE_TYPE.WALL-OUTER",
    "PRINT_TIME.LINE_TYPE.WALL-INNER",
    "PRINT_TIME.LINE_TYPE.FILL",
    "PRINT_TIME.LINE_TYPE.SKIN",
    "PRINT_TIME.LINE_TYPE.SUPPORT",
    "PRINT_TIME.LINE_TYPE.SUPPORT-INTERFACE",
    "PRINT_TIME.LINE_TYPE.RAFT",
    "PRINT_TIME.LINE_TYPE.SKIRT",
    "PRINT_TIME.LINE_TYPE.PRIME-TOWER",
    "PRINT_TIME.LINE_TYPE.TRAVEL",
    "PRINT_TIME.LINE_TYPE.RETRACTION",
};

void write_placeholder_line_type_times(std::ostream &out)
{
    for (const char *key : kIntamLineTypeTimes)
        out << ';' << key << ":0\n";
}

// Intam printers ignore unknown G/M codes inconsistently; comment M73 progress lines.
bool is_m73_command(const std::string &trimmed_lower)
{
    return boost::starts_with(trimmed_lower, "m73")
        && (trimmed_lower.size() == 3 || std::isspace(static_cast<unsigned char>(trimmed_lower[3])));
}

bool is_m204_command(const std::string &trimmed_lower)
{
    return boost::starts_with(trimmed_lower, "m204")
        && (trimmed_lower.size() == 4 || std::isspace(static_cast<unsigned char>(trimmed_lower[4])));
}

// INTAMSUITE / INTAM firmware only apply M204 S. Marlin 2 M204 P/T is ignored, which
// leaves the printer on a low EEPROM acceleration (~5x slower than the time estimate).
std::string rewrite_m204_for_intam(const std::string &trimmed)
{
    const auto        semi    = trimmed.find(';');
    std::string       code    = semi == std::string::npos ? trimmed : trimmed.substr(0, semi);
    const std::string comment = semi == std::string::npos ? std::string() : trimmed.substr(semi);
    boost::trim(code);

    int p = -1, t = -1, s = -1;
    std::istringstream in(code);
    std::string        tok;
    in >> tok; // M204
    while (in >> tok) {
        if (tok.size() < 2)
            continue;
        const char k = static_cast<char>(std::toupper(static_cast<unsigned char>(tok[0])));
        try {
            const int v = std::stoi(tok.substr(1));
            if (k == 'P')
                p = v;
            else if (k == 'T')
                t = v;
            else if (k == 'S')
                s = v;
        } catch (...) {}
    }

    const int accel = s >= 0 ? s : (p >= 0 ? p : t);
    if (accel < 0)
        return trimmed;
    if (s >= 0 && p < 0 && t < 0)
        return trimmed;

    std::string out = "M204 S" + std::to_string(accel);
    std::string rest = comment;
    boost::trim(rest);
    if (!rest.empty()) {
        out += " ";
        out += rest;
    }
    return out;
}

} // namespace

bool convert_gcode_to_intam_dialect(const std::string &in_path, const std::string &out_path)
{
    // UTF-8 paths: std::fstream uses the ANSI code page on Windows and breaks on
    // Chinese names (e.g. 刷架.gcode.intam.tmp). Same as the rest of libslic3r.
    boost::nowide::ifstream in(in_path, std::ios::binary);
    if (!in)
        return false;
    boost::nowide::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;

    std::string line;
    bool        saw_line_type_time = false;
    bool        saw_end_of_gcode   = false;
    bool        in_header          = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const std::string trimmed = trim_copy(line);
        const std::string lower   = to_lower(trimmed);

        if (lower == ";end of gcode")
            saw_end_of_gcode = true;

        if (lower == ";start_of_header")
            in_header = true;
        if (boost::starts_with(lower, ";print_time.line_type."))
            saw_line_type_time = true;

        if (in_header && lower == ";end_of_header") {
            if (!saw_line_type_time)
                write_placeholder_line_type_times(out);
            in_header = false;
            out << ";END_OF_HEADER\n";
            continue;
        }

        if (is_m73_command(lower))
            out << ';' << trimmed << '\n';
        else if (is_m204_command(lower))
            out << rewrite_m204_for_intam(trimmed) << '\n';
        else
            out << line << '\n';
    }

    // Cura / INTAMSUITE finalize with this marker after end gcode.
    if (!saw_end_of_gcode)
        out << ";End of Gcode\n";

    return out.good();
}

} // namespace Slic3r
