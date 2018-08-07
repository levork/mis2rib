/*
 * mis2rib.cpp
 *
 * Copyright (C) 2018 by Julian Fong. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * The RenderMan (R) Interface Procedures and RIB Protocol are:
 * Copyright 1988, 1989, Pixar. All rights reserved.
 * RenderMan (R) is a registered trademark of Pixar.
 *
 * This is mis2rib, a program for converting the Disney Moana Island
 * Scene data set to RenderMan Interface Bytestream .RIB files for use
 * in a RenderMan compliant renderer. In practice, this has primarily
 * been tested with one version of one renderer: Pixar's
 * PhotoRealistic RenderMan, release 22.
 */

#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include "json.hpp"

// for convenience
using json = nlohmann::json;
using namespace std;

struct Float3
{
    float x, y, z;
    Float3() {}
    Float3(const std::vector<float>& a) : x(a[0]), y(a[1]), z(a[2]) {}
    Float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

static Float3 cross(const Float3& a, const Float3& b)
{
    return Float3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

static float dot(const Float3& a, const Float3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static void normalize(Float3& a)
{
    float len = sqrtf(dot(a, a));
    a.x /= len;
    a.y /= len;
    a.z /= len;
}

////////////////////////////////////////////////////////////////////////////////
// OBJ file
////////////////////////////////////////////////////////////////////////////////

struct objstate
{
    string elementName;
    string currentName;
    string currentMaterial;
    vector<Float3> P;
    vector<Float3> N;
    map<int, int> Pmap, Prevmap;
    map<int, int> Nmap;
    int nverts = 0;
    int nfaces = 0;
    vector<int> facesize;
    vector<int> faceidx, faceNidx;
};

static void flushfaces(
    ofstream& ostr, struct objstate& s, const unordered_map<string, string>& materials)
{
    if (!s.facesize.empty())
    {
        // If the mesh is made of triangles, outputting a
        // Catmull-Clark subdiv is not a great idea
        bool polygons = (s.facesize[0] == 3);
        ostr << "AttributeBegin" << endl;
        auto mat = materials.find(s.currentMaterial);
        if (mat != materials.end())
        {
            string material = mat->second;

            // May need to perform two substitutions because of
            // displacement
            for (int i = 0; i < 2; ++i)
            {
                size_t pos = material.find("%", 0);
                if (pos != string::npos)
                {
                    string ptxfile = s.currentName + ".ptx";
                    material.replace(pos, 1, ptxfile);
                }
                else
                {
                    break;
                }
            }

            ostr << material << endl;
        }
        ostr << "    Attribute \"identifier\" \"string name\" \"" << s.currentName << "\"" << endl;
        ostr << "    Attribute \"identifier\" \"string object\" \"" << s.currentName << "\""
             << endl;
        if (polygons)
        {
            ostr << "    PointsPolygons [";
        }
        else
        {
            ostr << "    SubdivisionMesh \"catmull-clark\" [";
        }
        for (auto i = s.facesize.begin(); i != s.facesize.end(); ++i)
        {
            ostr << *i << ' ';
        }
        ostr << "] [";
        int maxvert = -1;
        for (auto i = s.faceidx.begin(); i != s.faceidx.end(); ++i)
        {
            ostr << *i << ' ';
            if (*i > maxvert) maxvert = *i;
        }
        ostr << "] ";
        if (!polygons)
        {
            ostr << "[\"interpolateboundary\"] [1 0] [1] [] ";
        }
        ostr << "\"vertex point P\" [";

        for (int i = 0; i <= maxvert; ++i)
        {
            auto j = s.Prevmap.find(i);
            if (j != s.Prevmap.end())
            {
                ostr << s.P[j->second].x << ' ' << s.P[j->second].y << ' ' << s.P[j->second].z
                     << ' ';
            }
            else
            {
                ostr << "-666 -666 -666 ";
            }
        }
        ostr << "] ";
        // I'm not terribly convinced that for subdivision meshes, the
        // provided normals are better than what RenderMan would
        // compute on the limit surface
        if (polygons)
        {
            if (!s.N.empty())
            {
                ostr << "\"vertex normal N\" [";
                for (int i = 0; i <= maxvert; ++i)
                {
                    int n = s.Nmap[i];
                    ostr << s.N[n].x << ' ' << s.N[n].y << ' ' << s.N[n].z << ' ';
                }
                ostr << "] ";
            }
        }
        ostr << "\"uniform float __faceindex\" [";
        for (int i = 0; i < (int)s.facesize.size(); ++i)
        {
            ostr << i << ' ';
        }
        ostr << "]" << endl;
        s.nverts = 0;
        s.nfaces = 0;
        s.Pmap.clear();
        s.Prevmap.clear();
        s.Nmap.clear();
        s.facesize.clear();
        s.faceidx.clear();
        ostr << "AttributeEnd" << endl;
    }
}

static int vertmap(struct objstate& s, int v)
{
    auto i = s.Pmap.find(v);
    if (i == s.Pmap.end())
    {
        s.Pmap[v] = s.nverts;
        s.Prevmap[s.nverts] = v;
        return s.nverts++;
    }
    else
    {
        return i->second;
    }
}

static void parseobj(
    const string& elementName,
    const unordered_map<string, string>& materials,
    ifstream& istr,
    ofstream& ostr)
{
    struct objstate s;
    s.elementName = elementName;
    string bufStr;
    while (getline(istr, bufStr))
    {
        if (!bufStr.empty() && bufStr[bufStr.length() - 1] == '\n')
        {
            bufStr.erase(bufStr.length() - 1);
        }
        if (bufStr.empty()) continue;
        const char* buf = bufStr.c_str();

        if (buf[0] != 'f')
        {
            // Flush faces in the queue if we encounter a new
            // directive
            flushfaces(ostr, s, materials);
        }

        if (buf[0] == '#')
        {
            // Comment
            ostr << buf << endl;
        }
        else if (buf[0] == 'g')
        {
            // Name of geometry, sometimes used for Ptx binding
            // purposes
            s.currentName = string(buf + 2);
        }
        else if (strncmp(buf, "usemtl ", 7) == 0)
        {
            // Material binding
            s.currentMaterial = string(buf + 7);
        }
        else if (buf[0] == 'v' && buf[1] == 'n')
        {
            // Normal
            float x, y, z;
            if (sscanf(buf, "vn %f %f %f", &x, &y, &z) == 3)
            {
                s.N.push_back(Float3(x, y, z));
            }
            else
            {
                cerr << "Bad normal directive:" << buf << endl;
            }
        }
        else if (buf[0] == 'v')
        {
            // Point
            float x, y, z;
            if (sscanf(buf, "v %f %f %f", &x, &y, &z) == 3)
            {
                s.P.push_back(Float3(x, y, z));
            }
            else
            {
                cerr << "Bad point directive:" << buf << endl;
            }
        }
        else if (buf[0] == 'f')
        {
            int v[4], vn[4];
            // Quad face
            if (sscanf(
                    buf,
                    "f %d//%d %d//%d %d//%d %d//%d",
                    &v[0],
                    &vn[0],
                    &v[1],
                    &vn[1],
                    &v[2],
                    &vn[2],
                    &v[3],
                    &vn[3]) == 8)
            {
                s.facesize.push_back(4);
                s.faceidx.push_back(vertmap(s, v[0] - 1));
                s.faceidx.push_back(vertmap(s, v[1] - 1));
                s.faceidx.push_back(vertmap(s, v[2] - 1));
                s.faceidx.push_back(vertmap(s, v[3] - 1));
                s.Nmap[vertmap(s, v[0] - 1)] = vn[0] - 1;
                s.Nmap[vertmap(s, v[1] - 1)] = vn[1] - 1;
                s.Nmap[vertmap(s, v[2] - 1)] = vn[2] - 1;
                s.Nmap[vertmap(s, v[3] - 1)] = vn[3] - 1;
                s.nfaces++;
            }
            // Triangle face
            else if (
                sscanf(
                    buf, "f %d//%d %d//%d %d//%d", &v[0], &vn[0], &v[1], &vn[1], &v[2], &vn[2]) ==
                6)
            {
                s.facesize.push_back(3);
                s.faceidx.push_back(vertmap(s, v[0] - 1));
                s.faceidx.push_back(vertmap(s, v[1] - 1));
                s.faceidx.push_back(vertmap(s, v[2] - 1));
                s.Nmap[vertmap(s, v[0] - 1)] = vn[0] - 1;
                s.Nmap[vertmap(s, v[1] - 1)] = vn[1] - 1;
                s.Nmap[vertmap(s, v[2] - 1)] = vn[2] - 1;
                s.nfaces++;
            }
            else
            {
                cerr << "Bad face directive:" << buf << endl;
            }
        }
    }
    flushfaces(ostr, s, materials);
}

static void objFile(
    ostream& ostr,
    const string& elementName,
    const string& filename,
    const unordered_map<string, string>& materials,
    bool isMaster)
{
    string ofilename = filename;

    size_t pos = ofilename.find(".obj", 0);
    if (pos != string::npos)
    {
        ofilename.replace(pos, 4, ".rib");
    }
    pos = ofilename.find("obj/", 0);
    if (pos != string::npos)
    {
        ofilename.replace(pos, 4, "rib/");
    }

    ifstream istr(filename.c_str());
    boost::filesystem::path p(ofilename);
    p.remove_filename();
    if (!boost::filesystem::exists(p))
    {
        boost::filesystem::create_directories(p);
    }

    ofstream ribostr(ofilename.c_str());
    parseobj(elementName, materials, istr, ribostr);

    if (!isMaster)
    {
        ostr << endl;
        ostr << "    #begin objFile " << filename << endl;
    }
    ostr << "    ReadArchive \"" << ofilename << "\"" << endl;
    if (!isMaster)
    {
        ostr << "    #end objFile " << filename << endl;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Material JSON file
////////////////////////////////////////////////////////////////////////////////

static void floatParam(ostream& ostr, const string& name, float f)
{
    ostr << " \"float " << name << "\" [" << f << "]";
}

static void outputTransform(ostream& ostr, const vector<float>& matrix)
{
    ostr << "ConcatTransform [" << matrix[0] << ' ' << matrix[1] << ' ' << matrix[2] << ' '
         << matrix[3] << ' ' << matrix[4] << ' ' << matrix[5] << ' ' << matrix[6] << ' '
         << matrix[7] << ' ' << matrix[8] << ' ' << matrix[9] << ' ' << matrix[10] << ' '
         << matrix[11] << ' ' << matrix[12] << ' ' << matrix[13] << ' ' << matrix[14] << ' '
         << matrix[15] << "]" << endl;
}

static string material(
    const string& elementName,
    const string& name,
    const json& j,
    unordered_map<string, string>& assignments)
{
    ostringstream ostr;

    // Check for displacement and color map
    if (j.find("displacementMap") != j.end() && j["displacementMap"] != "")
    {
        string displacementMap = j.at("displacementMap");
        ostr << "    Pattern \"PxrPtexture\" \"" << name
             << "DisplacementMap\" \"string filename\" \"" << displacementMap << "%\"" << endl;
        ostr << "    Displace \"PxrDisplace\" \"" << name << "\"";
        ostr << " \"reference float dispScalar\" [\"" << name << "DisplacementMap:resultR\"]"
             << endl;
    }

    bool ptxColor = false;
    if (j.find("colorMap") != j.end() && j["colorMap"] != "")
    {
        string colorMap = j.at("colorMap");
        if (!colorMap.empty() && colorMap[colorMap.length() - 1] != '/')
        {
            colorMap += "/";
        }
        // Encode a special token % which will be substituted by the
        // object name when we parse the obj file
        ostr << "    Pattern \"PxrPtexture\" \"" << name << "ColorMap\" \"string filename\" \""
             << colorMap << "%\"";
        // Convert to linear color space
        ostr << " \"int linearize\" [1]";
        ostr << endl;
        ptxColor = true;
    }

    ostr << "    Bxdf \"PxrSurface\" \"" << name << "\"";

    for (auto i = j.begin(); i != j.end(); ++i)
    {
        if (i.key() == "colorMap" || i.key() == "displacementMap")
        {
            // Handled above
        }
        else if (i.key() == "assignment")
        {
            // Unlike what the README implies, we do still need keep
            // track of assignments in order to correctly assign
            // materials to curves. We don't need to do this for the
            // OBJ files, but at this point we don't know whether the
            // material will be used on those.
            const vector<string>& s = i.value();
            for (auto k = s.begin(); k != s.end(); ++k)
            {
                assignments[*k] = name;
            }
        }
        else if (i.key() == "type")
        {
            // Need to do something here!
        }
        else if (i.key() == "baseColor")
        {
            if (ptxColor)
            {
                ostr << " \"reference color diffuseColor\" [\"" << name << "ColorMap:resultRGB\"]";
            }
            else
            {
                ostr << " \"color diffuseColor\" [" << i.value()[0] << ' ' << i.value()[1] << ' '
                     << i.value()[2] << "]";
            }
        }
        else if (i.key() == "diffTrans")
        {
            // An attempt to map diffuse trans to PxrSurface's diffuse
            // transmit behavior
            float diffTrans = i.value();
            if (diffTrans > 0)
            {
                ostr << " \"int diffuseDoubleSided\" [1]";
                floatParam(ostr, "diffuseGain", 0.5f * (2 - diffTrans));
                floatParam(ostr, "diffuseTransmitGain", 0.5f * diffTrans);
                if (ptxColor)
                {
                    ostr << " \"reference color diffuseTransmitColor\" [\"" << name
                         << "ColorMap:resultRGB\"]";
                }
                else if (j.find("baseColor") != j.end())
                {
                    ostr << " \"color diffuseTransmitColor\" [" << j["baseColor"][0] << ' '
                         << j["baseColor"][1] << ' ' << j["baseColor"][2] << "]";
                }
            }
        }
        else if (i.key() == "flatness")
        {
            // An attempt to turn flatness into some real subsurface
            float subsurfaceGain = i.value();
            if (subsurfaceGain != 0.0f)
            {
                floatParam(ostr, "subsurfaceGain", subsurfaceGain);
                if (ptxColor)
                {
                    ostr << " \"reference color subsurfaceColor\" [\"" << name
                         << "ColorMap:resultRGB\"]";
                }
                else if (j.find("baseColor") != j.end())
                {
                    ostr << " \"color subsurfaceColor\" [" << j["baseColor"][0] << ' '
                         << j["baseColor"][1] << ' ' << j["baseColor"][2] << "]";
                }
            }
        }
        else if (i.key() == "roughness")
        {
            // Disney BXDF roughness affects specular and diffuse
            // roughness lobes
            floatParam(ostr, "specularRoughness", i.value());
            floatParam(ostr, "diffuseRoughness", i.value());
        }
        else if (i.key() == "ior")
        {
            ostr << " \"color specularIor\" [" << i.value() << ' ' << i.value() << ' ' << i.value()
                 << "]";
        }
        else if (i.key() == "sheen")
        {
            // Map Disney BXDF sheen to PxrSurface fuzz
            floatParam(ostr, "fuzzGain", i.value());
        }
        else if (i.key() == "sheenTint")
        {
            float sheenTint = i.value();
            if (sheenTint != 0.0f && j.find("baseColor") != j.end())
            {
                if (sheenTint == 1.0f)
                {
                    if (ptxColor)
                    {
                        ostr << " \"reference color fuzzColor\" [\"" << name
                             << "ColorMap:resultRGB\"]";
                    }
                    else if (j.find("baseColor") != j.end())
                    {
                        ostr << " \"color fuzzColor\" [" << j["baseColor"][0] << ' '
                             << j["baseColor"][1] << ' ' << j["baseColor"][2] << "]";
                    }
                }
                else
                {
                    if (ptxColor)
                    {
                        // This is a situation we can't currently handle,
                        // so just punt and use the ptx color fully
                        ostr << " \"reference color fuzzColor\" [\"" << name
                             << "ColorMap:resultRGB\"]";
                    }
                    else
                    {
                        std::vector<float> fuzzColor = j["baseColor"];
                        fuzzColor[0] = (1.0f - sheenTint) + sheenTint * fuzzColor[0];
                        fuzzColor[1] = (1.0f - sheenTint) + sheenTint * fuzzColor[1];
                        fuzzColor[2] = (1.0f - sheenTint) + sheenTint * fuzzColor[2];
                        ostr << " \"color fuzzColor\" [" << fuzzColor[0] << ' ' << fuzzColor[1]
                             << ' ' << fuzzColor[2] << "]";
                    }
                }
            }
        }
        else if (i.key() == "alpha")
        {
            // No material sets this to value other than 1, so let's
            // just ignore it
        }
        else if (i.key() == "specTrans" || i.key() == "scatterDistance")
        {
            // Not handled yet!
        }
        else if (i.key() == "refractive")
        {
            // Attempt to turn on PxrSurface glass settings
            floatParam(ostr, "refractionGain", i.value());
            floatParam(ostr, "reflectionGain", i.value());
            floatParam(ostr, "diffuseGain", 0.0f);
            if (j.find("ior") != j.end())
            {
                floatParam(ostr, "glassIor", j["ior"]);
            }
            if (j.find("roughness") != j.end())
            {
                floatParam(ostr, "glassRoughness", j["roughness"]);
            }
        }
        else if (i.key() == "specularTint")
        {
            // The Disney BXDF is somewhere in between the artistic
            // and physical controls of PxrSurface. In particular, the
            // specularTint would be somewhat equivalent to setting
            // the artistic controls specularFaceColor and
            // specularEdgeColor to be a mix between white and the
            // baseColor, whereas the ior is a physical control.  For
            // now I'm just going to rely on the physical controls.
            // So while I'm outputting the artistic control values, I
            // haven't actually toggled on specularFresnelMode.
            float specularTint = i.value();
            if (specularTint != 0.0f && j.find("baseColor") != j.end())
            {
                std::vector<float> specularColor = j["baseColor"];
                specularColor[0] = (1.0f - specularTint) + specularTint * specularColor[0];
                specularColor[1] = (1.0f - specularTint) + specularTint * specularColor[1];
                specularColor[2] = (1.0f - specularTint) + specularTint * specularColor[2];
                ostr << " \"color specularFaceColor\" [" << specularColor[0] << ' '
                     << specularColor[1] << ' ' << specularColor[2] << "]";
                ostr << " \"color specularEdgeColor\" [" << specularColor[0] << ' '
                     << specularColor[1] << ' ' << specularColor[2] << "]";
            }
        }
        else if (i.key() == "metallic")
        {
            // Not even sure how to handle this
        }
        else if (i.key() == "clearcoat" || i.key() == "clearcoatGloss")
        {
            // Should try to map this to the secondary rough specular
            // lobe, maybe?
        }
        else if (i.key() == "anisotropic")
        {
            float anisotropic = i.value();
            if (anisotropic != 0.0f)
            {
                floatParam(ostr, i.key(), i.value());
            }
        }
        else
        {
            cerr << "Warning: unknown material key " << i.key() << endl;
        }
    }
    return ostr.str();
}

static void materialFile(
    const string& elementName,
    const string& filename,
    const json& j,
    unordered_map<string, string>& materials,
    unordered_map<string, string>& assignments)
{
    for (auto i = j.begin(); i != j.end(); ++i)
    {
        materials[i.key()] = material(elementName, i.key(), i.value(), assignments);
    }
}

////////////////////////////////////////////////////////////////////////////////

static void instancedArchive(
    ostream& ostr,
    const string& elementName,
    const string& primName,
    const json& j,
    const unordered_map<string, string>& materials)
{
    // Define the masters first
    ostr << "    #begin instance archive " << primName << endl;
    auto archives = j["archives"];
    for (auto i = archives.begin(); i != archives.end(); ++i)
    {
        string s = *i;
        ostr << "    ObjectBegin \"" << s << "\"" << endl;
        ostr << "    ";
        objFile(ostr, elementName, *i, materials, true);
        ostr << "    ObjectEnd" << endl;
    }

    // Create the instances
    string archiveFilename = j.at("jsonFile");
    ifstream archiveFile(archiveFilename.c_str());
    json archiveFileJSON;
    archiveFile >> archiveFileJSON;

    ostr << "    #begin instances " << endl;

    for (auto i = archiveFileJSON.begin(); i != archiveFileJSON.end(); ++i)
    {
        string master = i.key();
        json instances = i.value();
        for (auto k = instances.begin(); k != instances.end(); ++k)
        {
            string instance = k.key();
            ostr << "    AttributeBegin" << endl;
            ostr << "        Attribute \"identifier\" \"string name\" \"" << instance << "\""
                 << endl;
            ostr << "        ";
            outputTransform(ostr, k.value());
            ostr << "        ObjectInstance \"" << master << "\"" << endl;
            ostr << "    AttributeEnd" << endl;
        }
    }

    ostr << "    #end instances " << endl;
    ostr << "    #end instance archive " << j["jsonFile"] << endl;
}

static void instancedCurves(
    ostream& ostr,
    const string& elementName,
    const string& primName,
    const json& j,
    const unordered_map<string, string>& materials,
    const unordered_map<string, string>& assignments)
{
    float widthTip = j.at("widthTip");
    float widthRoot = j.at("widthRoot");

    // Create the curves
    string curveFilename = j.at("jsonFile");
    ifstream curveFile(curveFilename.c_str());
    json curveFileJSON;
    curveFile >> curveFileJSON;

    ostr << endl;
    ostr << "#begin curves " << primName << endl;

    ostr << "AttributeBegin" << endl;

    // We must look up the material assignment that was stored in the
    // material JSON
    auto assignment = assignments.find(primName);
    if (assignment != assignments.end())
    {
        auto mat = materials.find(assignment->second);
        if (mat != materials.end())
        {
            string material = mat->second;
            size_t pos = material.find("%", 0);
            if (pos != string::npos)
            {
                // There shouldn't be any ptx bindings found on curves
                // prims
                std::cerr << "Warning: illegal ptx binding found for curves material "
                          << assignment->second << endl;
                string ptxfile = mat->first + ".ptx";
                material.replace(pos, 1, ptxfile);
            }
            ostr << material << endl;
        }
    }

    // The curves data is actually b-spline cubic. In order to
    // interpolate the end points we must replicate them each three
    // times
    ostr << "    Basis \"b-spline\" 1 \"b-spline\" 1" << endl;
    ostr << "    Curves \"cubic\" [";
    for (auto i = curveFileJSON.begin(); i != curveFileJSON.end(); ++i)
    {
        json curve = i.value();
        ostr << curve.size() + 4 << ' ';
    }
    ostr << "] \"nonperiodic\" \"P\" [";
    for (auto i = curveFileJSON.begin(); i != curveFileJSON.end(); ++i)
    {
        json curve = i.value();
        auto k = curve.begin();
        // Repeat the first point twice
        ostr << (*k)[0].get<float>() << ' ' << (*k)[1].get<float>() << ' ' << (*k)[2].get<float>()
             << ' ';
        ostr << (*k)[0].get<float>() << ' ' << (*k)[1].get<float>() << ' ' << (*k)[2].get<float>()
             << ' ';
        for (; k != curve.end(); ++k)
        {
            ostr << (*k)[0].get<float>() << ' ' << (*k)[1].get<float>() << ' '
                 << (*k)[2].get<float>() << ' ';
        }
        // Repeat the last point twice
        --k;
        ostr << (*k)[0].get<float>() << ' ' << (*k)[1].get<float>() << ' ' << (*k)[2].get<float>()
             << ' ';
        ostr << (*k)[0].get<float>() << ' ' << (*k)[1].get<float>() << ' ' << (*k)[2].get<float>()
             << ' ';
    }
    ostr << "] \"varying float width\" [";
    for (auto i = curveFileJSON.begin(); i != curveFileJSON.end(); ++i)
    {
        json curve = i.value();
        ostr << widthRoot << ' ';
        for (int k = 0; k < curve.size() - 1; ++k)
        {
            float a = (float)k / (curve.size() - 1);
            ostr << widthRoot + a * (widthTip - widthRoot) << ' ';
        }
        ostr << widthTip << ' ';
        ostr << widthTip << ' ';
    }
    ostr << "]" << endl;
    ostr << "AttributeEnd" << endl;
    ostr << "#end curves " << curveFilename << endl;
}

static void instancedPrimitives(
    ostream& ostr,
    const string& elementName,
    const json& j,
    const unordered_map<string, string>& materials,
    const unordered_map<string, string>& assignments)
{
    ostr << endl;
    ostr << "    #begin instancedPrimitiveJsonFiles " << endl;

    for (auto i = j.begin(); i != j.end(); ++i)
    {
        string primName = i.key();
        json k = i.value();
        if (k.find("type") != k.end())
        {
            if (k["type"] == "curve")
            {
                instancedCurves(ostr, elementName, primName, k, materials, assignments);
            }
            else if (k["type"] == "archive")
            {
                instancedArchive(ostr, elementName, primName, k, materials);
            }
            else if (k["type"] == "element")
            {
                cerr << "HAS ELEMENT" << endl;
            }
        }
    }
    ostr << "    #end instancedPrimitiveJsonFiles " << endl;
}

////////////////////////////////////////////////////////////////////////////////

static void camera(ostream& ostr, const json& j)
{
    ostr << "Projection \"perspective\" \"fov\" [" << j["fov"].get<float>() << "]" << endl;
    std::vector<float> sw = j["screenwindow"];
    ostr << "ScreenWindow " << sw[0] << ' ' << sw[1] << ' ' << sw[2] << ' ' << sw[3] << endl;

    Float3 up(j["up"][0], j["up"][1], j["up"][2]);
    Float3 eye(j["eye"][0], j["eye"][1], j["eye"][2]);
    Float3 look(j["look"][0], j["look"][1], j["look"][2]);

    // RenderMan and Hyperion apparently disagree on the direction of
    // the X axis
    ostr << "Scale -1 1 1" << endl;

    // Standard lookat calculation
    Float3 z(look.x - eye.x, look.y - eye.y, look.z - eye.z);
    Float3 x = cross(up, z);
    Float3 y = cross(z, x);
    normalize(x);
    normalize(y);
    normalize(z);
    ostr << "ConcatTransform [" << x.x << ' ' << y.x << ' ' << z.x << " 0 " << x.y << ' ' << y.y
         << ' ' << z.y << " 0 " << x.z << ' ' << y.z << ' ' << z.z << " 0 " << -dot(x, eye) << ' '
         << -dot(y, eye) << ' ' << -dot(z, eye) << " 1]" << endl;
}

////////////////////////////////////////////////////////////////////////////////

static void light(ostream& ostr, const std::string& name, const json& j)
{
    std::string type = j["type"];
    if (type == "dome")
    {
        ostr << "AttributeBegin" << endl;
        ostr << "    ";
        outputTransform(ostr, j["translationMatrix"]);

        // Due to a difference in latlong coordinate systems
        // the following rotations appear to be required
        ostr << "    Rotate 90 0 1 0" << endl;
        ostr << "    Rotate -90 1 0 0" << endl;
        ostr << "    Light \"PxrDomeLight\" \"" << name << "\"";
        floatParam(ostr, "exposure", j["exposure"]);
        if (j.find("map") != j.end())
        {
            std::string mapfile = j["map"].get<string>();
            size_t pos = mapfile.find("exr", 0);
            if (pos != string::npos) mapfile.replace(pos, 3, "tx");
            pos = mapfile.find("island/", 0);
            if (pos != string::npos) mapfile.replace(pos, 7, "");
            ostr << " \"string lightColorMap\" [\"" << mapfile << "\"]";

            // I'm not sure if the colorMap is already gamma corrected
            // ostr << " \"color colorMapGamma\" [2.2 2.2 2.2]";
        }
        ostr << endl;
        ostr << "AttributeEnd" << endl;
    }
    else if (type == "quad")
    {
        ostr << "AttributeBegin" << endl;
        ostr << "    Attribute \"visibility\" \"int camera\" [0] \"int indirect\" [0]" << endl;
        ostr << "    ";
        outputTransform(ostr, j["translationMatrix"]);
        // Hyperion light sources apparently are aimed in the +Z direction
        ostr << "    Scale " << j["width"].get<float>() << ' ' << j["height"].get<float>() << " -1"
             << endl;
        ostr << "    Light \"PxrRectLight\" \"" << name << "\"";
        floatParam(ostr, "exposure", j["exposure"]);
        vector<float> lightColor = j["color"];

        // There's no gamma for non-texture mapped color, so we need
        // to correct here
        ostr << " \"color lightColor\" [" << pow(lightColor[0], 2.2) << ' '
             << pow(lightColor[1], 2.2) << ' ' << pow(lightColor[2], 2.2) << "]";
        ostr << endl;
        ostr << "AttributeEnd" << endl;
    }
}

static void lights(ostream& ostr, const json& j)
{
    for (auto i = j.begin(); i != j.end(); ++i)
    {
        light(ostr, i.key(), i.value());
    }
}

////////////////////////////////////////////////////////////////////////////////

static void element(ostream& ostr, const json& j)
{
    try
    {
        string elementName = j.at("name");
        ostr << "ObjectBegin \"" << elementName << "\"" << endl;
        ostr << "    Attribute \"identifier\" \"string object\" \"" << elementName << "\"" << endl;

        // Define the materials
        unordered_map<string, string> materials;
        unordered_map<string, string> assignments;
        string matFilename = j.at("matFile");
        ifstream matFile(matFilename.c_str());
        json matFileJSON;
        matFile >> matFileJSON;
        materialFile(elementName, matFilename, matFileJSON, materials, assignments);

        // Load the element excluding instances
        string filename = j.at("geomObjFile");
        objFile(ostr, elementName, filename, materials, false);

        // Load instances
        if (j.find("instancedPrimitiveJsonFiles") != j.end())
        {
            instancedPrimitives(
                ostr, elementName, j["instancedPrimitiveJsonFiles"], materials, assignments);
        }

        ostr << "ObjectEnd" << endl;
        ostr << "AttributeBegin" << endl;
        if (j.find("transformMatrix") != j.end())
        {
            ostr << "    Attribute \"identifier\" \"string name\" \"" << elementName << "\""
                 << endl;
            // There's some buggy transforms in the data set..
            if (!j["transformMatrix"].is_null())
            {
                ostr << "    ";
                outputTransform(ostr, j["transformMatrix"]);
            }
        }
        ostr << "    ObjectInstance \"" << elementName << "\"" << endl;
        ostr << "AttributeEnd" << endl;

        if (j.find("instancedCopies") != j.end())
        {
            auto instances = j["instancedCopies"];
            for (auto k = instances.begin(); k != instances.end(); ++k)
            {
                ostr << "AttributeBegin" << endl;
                std::string instanceName = k.key();
                json instance = k.value();

                // There's some buggy transforms in the data set..
                if (!instance["transformMatrix"].is_null())
                {
                    ostr << "    ";
                    outputTransform(ostr, instance["transformMatrix"]);
                }

                // Some "instancedCopies" aren't actually instances;
                // they are actually full geometry representations in
                // their own right. The coral appears to work this way.
                if (instance.find("geomObjFile") != instance.end())
                {
                    string filename = instance.at("geomObjFile");
                    objFile(ostr, instanceName, filename, materials, false);

                    // Load instances
                    if (instance.find("instancedPrimitiveJsonFiles") != instance.end())
                    {
                        instancedPrimitives(
                            ostr,
                            instanceName,
                            instance["instancedPrimitiveJsonFiles"],
                            materials,
                            assignments);
                    }
                }
                // Here we have a more reasonable "true" object
                // instance
                else
                {
                    ostr << "    Attribute \"identifier\" \"string name\" \"" << instanceName
                         << "\"" << endl;
                    ostr << "    ObjectInstance \"" << elementName << "\"" << endl;
                }
                ostr << "AttributeEnd" << endl;
            }
        }
    }
    catch (json::out_of_range& e)
    {
        cerr << e.what() << endl;
    }
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        cerr << "Usage: " << argv[0] << " (camera|element) filename.json" << endl;
        exit(1);
    }

    string type(argv[1]);
    if (type == "camera")
    {
        ifstream i(argv[2]);
        json j;
        i >> j;
        camera(cout, j);
    }
    else if (type == "lights")
    {
        ifstream i(argv[2]);
        json j;
        i >> j;
        lights(cout, j);
    }
    else if (type == "element")
    {
        ifstream i(argv[2]);
        json j;
        i >> j;
        element(cout, j);
    }
    else
    {
        cerr << "Unknown type " << type << ", must be camera, lights, or element";
        exit(1);
    }
}
