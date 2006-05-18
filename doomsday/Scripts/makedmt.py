#!/usr/bin/python
# Python script for generating Doomsday map data structs and definitions.

import sys, string


def error(line, cause):
    err = sys.stderr
    err.write("At input line \"%s\":\nProcessing aborted: %s\n" % (line.rstrip(), cause))
    sys.exit(1)


def add_comment(line, comment):
    if comment == '':
       return line
    if len(line) < 39:
        line += ' ' * (39 - len(line))
    else:
        line += ' '
    return line + comment

    
def println(f, line, comment=''):
    f.write(add_comment(line, comment) + "\n")


current = None
verbatim = None

internal_header_filename = 'p_maptypes.h'
public_header_filename = 'dd_maptypes.h'

internal_file = file(internal_header_filename, 'w')
public_file = file(public_header_filename, 'w')

banner = "/* Generated by " + string.join(sys.argv, ' ') + " */\n\n"
internal_file.write(banner)
public_file.write(banner)

# Begin writing to the internal header.
internal_file.write("#ifndef __DOOMSDAY_PLAY_MAP_DATA_TYPES_H__\n")
internal_file.write("#define __DOOMSDAY_PLAY_MAP_DATA_TYPES_H__\n\n")
internal_file.write("#include \"p_data.h\"\n\n")

# Begin writing to the public header.
public_file.write("#ifndef __DOOMSDAY_PLAY_PUBLIC_MAP_DATA_TYPES_H__\n")
public_file.write("#define __DOOMSDAY_PLAY_PUBLIC_MAP_DATA_TYPES_H__\n\n")

for input_line in sys.stdin.readlines():
    line = input_line.strip()
    
    if verbatim:
        if line == 'end':
            println(verbatim, "")
            verbatim = None
        else:
            println(verbatim, input_line.rstrip())
        continue
    
    # Skip empty lines and comments.
    if len(line) == 0 or line[0] == '#':
        continue
        
    # If there is a comment on the line, get rid of it.
    comment_stripped = line
    line_comment = ''
    if '//' in comment_stripped:
        index = comment_stripped.index('//')
        line_comment = comment_stripped[index:]
        comment_stripped = comment_stripped[:index]
        
    tokens = comment_stripped.split()
    count = len(tokens)
    
    if current is None:
        if count == 1:
            if tokens[0] == 'internal':
                verbatim = internal_file
            elif tokens[0] == 'public':
                verbatim = public_file
        elif count == 2:
            if tokens[0] == 'struct':
                current = tokens[1]
                println(internal_file, "typedef struct %s_s {" % current, line_comment)
                println(internal_file, "    runtime_mapdata_header_t header;", '')
        else:
            error(input_line, 'syntax error')
    else:
        if count == 3:
            # Use "-" to omit the DDVT type declaration (internal usage only).
            if tokens[0] != '-':
                println(public_file, 
                    "#define DMT_%s_%s DDVT_%s" % (current.upper(),
                                                   tokens[2].upper(),
                                                   tokens[0].upper()), line_comment)
            # Determine the C type.
            c_type = tokens[1]
            indexed = ''
            if '[' in c_type:
                pos = c_type.index('[')
                indexed = c_type[pos:]
                c_type = c_type[:pos]
                
            if '_s' in c_type:
                c_type = 'struct ' + c_type
            
            # Add some visual padding to align the members.
            padding = 24 - len(c_type) - 4
            if padding < 1: padding = 1
            
            println(internal_file, "    %s%s%s%s;" % (c_type, ' '*padding, tokens[2], indexed), 
                    line_comment)
        elif count == 1:
            if tokens[0] == 'end':
                println(internal_file, "} %s_t;\n" % current, line_comment)
                println(public_file, "", line_comment)
                current = None
            else:
                error(input_line, 'syntax error')
        else:
            error(input_line, "unknown definition in struct %s" % current)
        
# End the header files.
internal_file.write("#endif\n")
public_file.write("#endif\n")
