INTRODUCTION
------------

This is mis2rib, a program for converting the Disney Moana Island
Scene data set (https://www.disneyanimation.com/technology/datasets)
to RenderMan Interface Bytestream .RIB files for use in a RenderMan
compliant renderer. In practice, this has primarily been tested with
one version of one renderer: Pixar's PhotoRealistic RenderMan, release
22.

USAGE
-----

Unpack the Disney Moana Island Scene base package.

Copy the shell script mis2rib.sh and island.rib to the root directory
of the island scene package.

Compile mis2rib.cpp and create the executable mis2rib. It requires
boost and JSON for Modern C++ (https://github.com/nlohmann/json); the
latter requires a fairly modern C++ compiler. The compile line on my
system looks something like this (using Intel's C++ compiler):

icpc -O3 -std=c++11 -I/include/path/to/boost -I/include/path/to/json.hpp \
    -Wall mis2rib.cpp -o mis2rib \
    -L/library/path/to/boost/ -lboost_filesystem -lboost_system 

Install mis2rib in the root directory of the island scene package.

From the same directory, run mis2rib.sh. This takes around half an
hour to generate all necessary RIB files. It will also run the txmake
utility to convert the latlong environment map for the domelight from
EXR to Pixar format, so there will be an assumption that the
environment variable RMANTREE points to an installation of RenderMan.

You can now render island.rib.

prman island.rib

By default, it is set up to use the camera shotCam. This file should
be edited to render from the viewpoint of other cameras, or to alter
the resolution, integrator settings, etc.

KNOWN ISSUES
------------

As mentioned above, this has only been tested in Pixar's
PhotoRealistic RenderMan, versions 22 (21 should also work), so much
of the RIB output is specific to RIS. However, the only part of the
generated RIB files that are truly specific to RIS are the material
definitions; changing that part of the C++ program and the wrapper RIB
file island.rib should be straightforward in order to adapt this for
other RenderMan compliant renderers.

Because PRMan does not ship with a recent version of the Disney Bxdf
which implements a BSDF, and usage of the BSDF is critical to the look
of the vegetation, the conversion process tries to convert the
material definitions to use the PxrSurface Bxdf instead. The mapping
of one set of parameters to another set is not necessarily
straightforward and there may be significant look differences as a
result. I hope to improve this as time permits.

RenderMan 22 no longer implements flat curves which face the camera,
and this is a significant cause of illumination differences. This may
be dealt with in a future release by generating curves with normals
which face the camera, although this will require re-converting the
data if the camera changes.

There are some Ptx issues in the original version of the Moana Island
Data set which may cause some vegetation to turn pink due to missing
or wrong Ptx bindings, and is also known to behind one set of NaN
artifacts. This is a known problem in the data set which will be
fixed. Similarly, there is a bug in the original data set with respect
to the placement of the isIronwoodB element.

The handling of the conversion between SRGB and linear space may not
be complete.

The fill lights are not selectively linked to the geometry; this is
the cause of some of the odd reflections in the water.
