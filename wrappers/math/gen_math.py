# Copyright (C) 2007 Maryland Robotics Club
# Copyright (C) 2007 Joseph Lisee <jlisee@umd.edu>
# All rights reserved.
#
# Author: Joseph Lisee <jlisee@umd.edu>
# File:  wrappers/math/gen_math.py

# STD Imports
import os

# Library Imports
from pyplusplus.module_builder import call_policies
from pygccxml import declarations

# Project Imports
import buildfiles.wrap as wrap

def generate(module_builder, local_ns, global_ns):
    """
    global_ns: is the module builder for the entire library
    local_ns: is the namespace that coresponds to the given namespace
    """
    base_path = os.path.join(os.environ['RAM_SVN_DIR'],'packages','math',
                             'include')

    classes = []

    # Find all the classes to wrap
    Radian = local_ns.class_('Radian')
    Degree = local_ns.class_('Degree')
    Vector3 = local_ns.class_('Vector3')
    Quaternion = local_ns.class_('Quaternion')
    Matrix3 = local_ns.class_('Matrix3')

    # Include them
    Radian.include()
    Degree.include()
    Vector3.include()
    Quaternion.include()
    Matrix3.include()

    classes.extend([Radian, Degree, Vector3, Quaternion, Matrix3])

    # Map operator<< to __str__
    wrap.str_from_ostream(local_ns)


    # Fix '[]' operators on matrices
    c = Matrix3.operators('[]')
    c.call_policies= call_policies.convert_array_to_tuple(3, \
        call_policies.memory_managers.none)
    c.include()
    c.documentation = wrap.docit("Return Type Change", "None",
                                 "Tuple with 3 floats's (the matrix 'line')")

    # Handle the 'ptr' functions
    wrap.fix_pointer_returns([Vector3, Quaternion, Matrix3],
                             ignore_names = ['ptr'])
    wrap.fix_pointer_args([Vector3, Quaternion, Matrix3])

    # Remove float -> Radian/Degree implicit conversions
    Degree.constructor(arg_types = ['double']).allow_implicit_conversion = False
    Radian.constructor(arg_types = ['double']).allow_implicit_conversion = False

    # Wrap Events
    eventsFound = False
    for cls in local_ns.classes(function= lambda x: x.name.endswith('Event'),
                                allow_empty = True):
        cls.include()
        classes.append(cls)

    if eventsFound:
        wrap.make_already_exposed(global_ns, 'ram::core', ['Event'])

    # Append the approaite include files
    wrap.add_needed_includes(classes)
    Quaternion.include_files.append(Matrix3.location.file_name)
    # Remove implicit conversions
    wrap.set_implicit_conversions([Vector3, Quaternion, Matrix3],
                                  False)

    return ['math/include/Math.h']
