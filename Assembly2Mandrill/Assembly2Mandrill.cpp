#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <cassert>
#include "..\lib\pugixml\src\pugixml.hpp"

using std::string;

const string TAG = "chdt";

bool str_equal(string a, const char b[])
{
    return a.compare(b) == 0;
}

string lowercase(string s)
{
    string result = "";
    for (string::iterator it = s.begin(); it != s.end(); ++it)
    {
        char c = *it;
        result += tolower(c);
    }
    return result;
}

enum field_type
{
    UNDEFINED,
    REAL,
    BYTE_INT,
    SHORT_INT,
    LONG_INT,
    BYTE_FLAGS,
    WORD_FLAGS,
    LONG_FLAGS,
    CHAR_ENUM,
    ENUM,
    LONG_ENUM,
    TAG_BLOCK,
    COLOR,
    TAG_REF,
    DATA_REF,
    STRING,
    STRING_ID,
    COMMENT,
    ANGLE,
    RECT,
    // these two don't have any representation on the mandrill side; the values end up just being string literals
    OPTION,
    BIT,
};

field_type get_field_type(string assembly_type)
{
    if (str_equal(assembly_type, "float32")) return REAL;
    if (str_equal(assembly_type, "int8") || str_equal(assembly_type, "uint8")) return BYTE_INT;
    if (str_equal(assembly_type, "int16") || str_equal(assembly_type, "uint16")) return SHORT_INT;
    if (str_equal(assembly_type, "int32") || str_equal(assembly_type, "uint32")) return LONG_INT;
    if (str_equal(assembly_type, "flags8")) return BYTE_FLAGS;
    if (str_equal(assembly_type, "flags16")) return WORD_FLAGS;
    if (str_equal(assembly_type, "flags32")) return LONG_FLAGS;
    if (str_equal(assembly_type, "enum8")) return CHAR_ENUM;
    if (str_equal(assembly_type, "enum16")) return ENUM;
    if (str_equal(assembly_type, "enum32")) return LONG_ENUM;
    if (str_equal(assembly_type, "tagblock")) return TAG_BLOCK;
    if (str_equal(assembly_type, "colorf")) return COLOR;
    if (str_equal(assembly_type, "tagref")) return TAG_REF;
    if (str_equal(assembly_type, "dataref")) return DATA_REF;
    if (str_equal(assembly_type, "ascii")) return STRING;
    if (str_equal(assembly_type, "stringid")) return STRING_ID;
    if (str_equal(assembly_type, "comment")) return COMMENT;
    if (str_equal(assembly_type, "option")) return OPTION;
    if (str_equal(assembly_type, "bit")) return BIT;
    if (str_equal(assembly_type, "degree")) return ANGLE;
    if (str_equal(assembly_type, "rect16")) return RECT;
    return UNDEFINED;
}

string get_mandrill_type(field_type type)
{
    switch (type)
    {
    case REAL: return "_field_real";
    case BYTE_INT: return "_field_byte_integer";
    case SHORT_INT: return "_field_short_integer";
    case LONG_INT: return "_field_long_integer";
    case BYTE_FLAGS: return "_field_byte_flags";
    case WORD_FLAGS: return "_field_word_flags";
    case LONG_FLAGS: return "_field_long_flags";
    case CHAR_ENUM: return "_field_char_enum";
    case ENUM: return "_field_enum";
    case LONG_ENUM: return "_field_long_enum";
    case TAG_BLOCK: return "_field_block";
    case COLOR: return "_field_real_argb_color";
    case TAG_REF: return "_field_tag_reference";
    case DATA_REF: return "_field_data";
    case STRING: return "_field_string";
    case STRING_ID: return "_field_string_id";
    case COMMENT: return "_field_explanation";
    case ANGLE: return "_field_angle";
    case RECT: return "_field_rectangle_2d";
    }
    return "undefined_fixme";
}

struct structure
{
    field_type type = UNDEFINED; // should be a tag block or enum or flag
    string definition_name = "";
    string field_name_value = "";
    string field_string = "";
    string prefix = "";
    string children = "";
    string postfix = "";
};

structure root_tag_block;
std::map<int, structure> hash_to_struct_map;
std::map<string, int> definition_name_usage_map;

bool is_enum_or_flags(field_type type)
{
    return type == BYTE_FLAGS || type == WORD_FLAGS || type == LONG_FLAGS || type == CHAR_ENUM || type == ENUM || type == LONG_ENUM;
}

bool is_useless(string assembly_type)
{
    return str_equal(assembly_type, "revisions") ||
        str_equal(assembly_type, "revision") ||
        str_equal(assembly_type, "plugin");
}

string format_definition_name(string s, field_type type)
{
    string result = TAG + "_";
    for (string::iterator it = s.begin(); it != s.end(); ++it)
    {
        char c = *it;
        if (c == '\'' || c == '"')
            continue;

        if (isspace(c) || c == '-')
            result += '_';
        else
            result += tolower(c);
    }
    if (type == TAG_BLOCK)
        result += "_block";
    else
        result += "_definition";
    return result;
}

string split_into_lined_string(string s)
{
    string result = "";
    for (string::iterator it = s.begin(); it != s.end(); ++it)
    {
        char c = *it;
        if (c == '\n')
            result += "\"\n\\n\"";
        else if (c == '\"')
            result += "\\\"";
        else
            result += tolower(c);
    }
    return result;
}

string format_mandrill_field(field_type type, string definition_name, string field_name_value)
{
    string mandrill_type = get_mandrill_type(type);
    string result;
    if (type == BIT || type == OPTION)
        result = '\"' + field_name_value + "\",\n";
    else
    {
        result = "{ " + mandrill_type + ", \"" + field_name_value + "\"";
        if (type == TAG_BLOCK)
            result += ", &" + definition_name + "_block },\n";
        else if (type == TAG_REF)
            result += ", &tagref_fixme },\n";
        else if (is_enum_or_flags(type))
            result += ", &" + definition_name + " },\n";
        else
            result += " },\n";
    }
    return result;
}

void set_prefix_and_postfix(structure& tag_struct)
{
    if (tag_struct.type == TAG_BLOCK)
    {
        tag_struct.prefix = "TAG_BLOCK(" + tag_struct.definition_name + ", 65536)\n{\n";
        tag_struct.postfix = "\t{ _field_terminator }\n};\n";
    }
    else if (is_enum_or_flags(tag_struct.type))
    {
        tag_struct.prefix = "STRINGS(" + tag_struct.definition_name + ")\n{\n";
        tag_struct.postfix = "};\nSTRING_LIST(" + tag_struct.definition_name + ", " + tag_struct.definition_name + "_strings, _countof(" + tag_struct.definition_name + "_strings));\n";
    }
}

string parse_node(pugi::xml_node node)
{
    structure tag_struct;
    string name = node.name();
    field_type type = get_field_type(lowercase(name));
    string mandrill_type = get_mandrill_type(type);
    string field_name_value = lowercase(node.attribute("name").value());
    string definition_name = format_definition_name(field_name_value, type);

    if (type == COMMENT)
    {
        // TODO comments
        //out << "{ " << mandrill_type << ", \"" << lowercase(node.attribute("title").value()) << "\", \"" << split_into_lined_string(node.child_value()) << "\" },\n";
        return "";
    }

    size_t data_hash = -1;
    if (type == TAG_BLOCK || is_enum_or_flags(type))
    {
        string children = "";
        for (pugi::xml_node child : node.children())
        {
            // yes, we loop through the children of tag structures twice, but idgaf, performance isn't a priority for this
            string field_string = parse_node(child);
            children += '\t' + field_string;
        }

        tag_struct.type = type;
        tag_struct.definition_name = definition_name;
        tag_struct.field_name_value = field_name_value;
        tag_struct.children = children;

        data_hash = std::hash<string>()(children);
        if (hash_to_struct_map.find(data_hash) != hash_to_struct_map.end())
        {
            definition_name = hash_to_struct_map[data_hash].definition_name;
        }
        else
        {
            if (definition_name_usage_map.find(definition_name) != definition_name_usage_map.end())
            {
                int num = ++definition_name_usage_map[definition_name];
                definition_name += "$" + std::to_string(num);
            }
            else
            {
                definition_name_usage_map[definition_name] = 1;
            }
        }
        tag_struct.definition_name = definition_name;
        set_prefix_and_postfix(tag_struct);
    }

    string field_string = format_mandrill_field(type, definition_name, field_name_value);
    tag_struct.field_string = field_string;

    if (data_hash != -1)
        hash_to_struct_map[data_hash] = tag_struct;

    return field_string;
}

void parse_node_and_children(pugi::xml_node node, int level)
{
    if (!is_useless(node.name()))
    {
        string field_string = parse_node(node);
        if (level == 1)
            root_tag_block.children += '\t' + field_string;
    }

    for (pugi::xml_node child : node.children())
    {
        parse_node_and_children(child, level + 1);
    }
}

int main(int argc, char* argv[], char* envp[])
{
    assert(argc >= 3, "not enough arguments");

    string input_file_path = argv[1];
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(input_file_path.c_str());

    assert(result, "couldn't load input xml");

    string output_file_path = argv[2];
    std::ofstream output(output_file_path);

    assert(output, "couldn't open output file path");

    root_tag_block.type = TAG_BLOCK;
    root_tag_block.definition_name = TAG + "_block";
    set_prefix_and_postfix(root_tag_block);

    parse_node_and_children(doc.child("plugin"), 0);

    output << '\n';

    for (std::map<int, structure>::iterator it = hash_to_struct_map.begin(); it != hash_to_struct_map.end(); ++it)
    {
        structure tag_struct = it->second;
        output << tag_struct.prefix;
        output << tag_struct.children;
        output << tag_struct.postfix;
        output << '\n';
    }

    output << root_tag_block.prefix;
    output << root_tag_block.children;
    output << root_tag_block.postfix;
    output.close();
}