//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "MooseObject.h"
#include "MeshMetaDataInterface.h"
#include "MooseApp.h"

// Included so mesh generators don't need to include this when constructing MeshBase objects
#include "MooseMesh.h"

#include "libmesh/mesh_base.h"
#include "libmesh/parameters.h"

class MooseMesh;
namespace libMesh
{
class ReplicatedMesh;
class DistributedMesh;
}

/**
 * MeshGenerators are objects that can modify or add to an existing mesh.
 */
class MeshGenerator : public MooseObject, public MeshMetaDataInterface
{
public:
  /**
   * Constructor
   *
   * @param parameters The parameters object holding data for the class to use.
   */
  static InputParameters validParams();

  MeshGenerator(const InputParameters & parameters);

  /**
   * Generate / modify the mesh
   */
  virtual std::unique_ptr<MeshBase> generate() = 0;

  /**
   * Internal generation method - this is what is actually called
   * within MooseApp to execute the MeshGenerator.
   */
  std::unique_ptr<MeshBase> generateInternal();

  /**
   * Return the MeshGenerators that must run before this MeshGenerator
   */
  std::vector<std::string> & getDependencies() { return _depends_on; }

protected:
  /**
   * Methods for writing out attributes to the mesh meta-data store, which can be retrieved from
   * most other MOOSE systems and is recoverable.
   */
  template <typename T>
  T & declareMeshProperty(const std::string & data_name);

  template <typename T>
  T & declareMeshProperty(const std::string & data_name, const T & init_value);

  /**
   * Checks the sanity of the input parameters that are related to mesh metadata retainment
   * @param has_input whether the mesh generator takes another mesh generator as an input
   * @param retain_all_input_mesh_metadata whether to retain all the mesh metadata from the input
   * mesh
   * @param selected_mesh_metadata_to_retain list of selected mesh metadata from the input mesh that
   * need to be retained
   */
  void checkMeshMetadataForwardingSetting(
      const bool has_input,
      const bool retain_all_input_mesh_metadata,
      const std::vector<std::string> selected_mesh_metadata_to_retain);

  /// enum class for all types that can be used as mesh metadata
  enum type_names_enum
  {
    typeVectorUnsignedInt,
    typeDouble,
    typeVectorDouble,
    typeUnsignedShort,
    typeUnsignedInt,
    typeBool,
    typeUnsignedLongLong,
    typeString,
    typeInt,
    typePoint,
    typeVectorInt,
    typeVectorUnsignedShort,
    typeVectorUnsignedLongLong,
    typeVectorPoint,
    typeVectorVectorDouble,
    typeMapStringPairUnsignedShortUnsignedLongLong,
    typeShort,
    typeMapSubdomainIdTypeVectorVectorSubdomainIdType,
    typeMapSubdomainIdTypeVectorVectorString,
    typeVectorString,
    typeVectorVectorShort,
    typeVectorVectorString,
    typeMapBoundaryIDRealVectorValue
  };
  /// This collects all the type names that are declared as Mesh Metadata
  const std::map<std::string, type_names_enum> _type_name_map = {
      {"std::vector<unsigned int>", typeVectorUnsignedInt},
      {"double", typeDouble},
      {"std::vector<double>", typeVectorDouble},
      {"unsigned short", typeUnsignedShort},
      {"unsigned int", typeUnsignedInt},
      {"bool", typeBool},
      {"unsigned long long", typeUnsignedLongLong},
      {"std::string", typeString},
      {"int", typeInt},
      {"libMesh::Point", typePoint},
      {"std::vector<int>", typeVectorInt},
      {"std::vector<unsigned short>", typeVectorUnsignedShort},
      {"std::vector<unsigned long long>", typeVectorUnsignedLongLong},
      {"std::vector<libMesh::Point>", typeVectorPoint},
      {"std::vector<std::vector<double>>", typeVectorVectorDouble},
      {"std::map<std::string, std::pair<unsigned short, unsigned long long>, "
       "std::less<std::string>, std::allocator<std::pair<std::stringconst, std::pair<unsigned "
       "short, unsigned long long> > > >",
       typeMapStringPairUnsignedShortUnsignedLongLong},
      {"short", typeShort},
      {"std::map<unsigned short, std::vector<std::vector<unsigned short>>, std::less<unsigned "
       "short>, std::allocator<std::pair<unsigned short const, std::vector<std::vector<unsigned "
       "short>> > > >",
       typeMapSubdomainIdTypeVectorVectorSubdomainIdType},
      {"std::map<unsigned short, std::vector<std::vector<std::string>>, std::less<unsigned short>, "
       "std::allocator<std::pair<unsigned short const, std::vector<std::vector<std::string>> > > >",
       typeMapSubdomainIdTypeVectorVectorString},
      {"std::vector<std::string>", typeVectorString},
      {"std::vector<std::vector<unsigned short>>", typeVectorVectorShort},
      {"std::vector<std::vector<std::string>>", typeVectorVectorString},
      {"std::map<short, libMesh::VectorValue<double>, std::less<short>, "
       "std::allocator<std::pair<short const, libMesh::VectorValue<double> > > >",
       typeMapBoundaryIDRealVectorValue}};

  /**
   * Checks if the declared forwarded mesh metadata has the correct type
   * @param temp_value the empty declared metadata that contains the type information
   * @param type_name the reference mesh metadata type name to be compared to
   * @return whether the declared forwarded mesh metadata has the same type as described by the
   * reference
   */
  template <typename T>
  bool meshMetadataTypeCheck(T /* temp_value */, const std::string type_name);

  /**
   * Declares a mesh metadata that is a copy of another mesh's metadata
   * @param data_name name of the mesh metadata to be copied
   * @param input_mg_name name of the input mesh that contains the source mesh metadata
   * @return type index enum of the mesh metadata declared; the index is based on _type_name_map
   */
  type_names_enum declareForwardedMeshProperty(const std::string data_name,
                                               const std::string input_mg_name);

  /**
   * Declares multiple mesh metadata that copy all the mesh metadata of the input mesh
   * @param input_name name of the input mesh that contains the source mesh metadata
   * @param metadata_names list of the names of all the mesh metadata that are declared
   * @param metadata_types list of the type indices of all the mesh metadata that are declared
   */
  void declareAllForwardedMeshMetadata(const MeshGeneratorName input_name,
                                       std::vector<std::string> & metadata_names,
                                       std::vector<type_names_enum> & metadata_types);

  /**
   * Declares multiple mesh metadata that copy selected mesh metadata of the input mesh
   * @param input_name name of the input mesh that contains the source mesh metadata
   * @param selected_metadata_names list of the mesh metadata names in the input mesh that need to
   * be copied
   * @param metadata_names list of the names of the selected mesh metadata that are declared
   * @param metadata_types list of the type indices of the selected mesh metadata that are declared
   */
  void declareSelectedForwardedMeshMetadata(const MeshGeneratorName input_name,
                                            const std::vector<std::string> selected_metadata_names,
                                            std::vector<std::string> & metadata_names,
                                            std::vector<type_names_enum> & metadata_types);

  /**
   * Sets one declared mesh metadata's value using a mesh metadata value from the input mesh
   * @param data_name name of the mesh metadata to be copied
   * @param input_mg_name name of the input mesh that contains the source mesh metadata
   * @param type_id type index of the mesh metadata to be set; the index is based on _type_name_map
   */
  void setForwardedMeshProperty(const std::string data_name,
                                const std::string input_mg_name,
                                const type_names_enum type_id);

  /**
   * Sets a series of declared mesh metadata's values using mesh metadata values from the input mesh
   * @param input_name name of the input mesh that contains the source mesh metadata
   * @param metadata_names list of the names of the selected mesh metadata that need to be set
   * @param metadata_types list of the type indices of the selected mesh metadata that need to be
   * set
   */
  void setForwardedMeshMetadata(const MeshGeneratorName input_name,
                                const std::vector<std::string> metadata_names,
                                const std::vector<type_names_enum> metadata_types);

  /**
   * Method for updating attributes to the mesh meta-data store, which can only be invoked in
   * the MeshGenerator generate routine only if the mesh generator property has already been
   * declared.
   */
  template <typename T>
  T & setMeshProperty(const std::string & data_name, const T & data_value);

  /**
   * Takes the name of a MeshGeneratorName parameter and then gets a pointer to the
   * Mesh that MeshGenerator is going to create.
   *
   * That MeshGenerator is made to be a dependency of this one, so
   * will generate() its mesh first.
   *
   * @param allow_invalid If true, will allow for invalid parameters and will return a nullptr
   * mesh if said parameter does not exist
   * @return The Mesh generated by that MeshGenerator
   *
   * NOTE: You MUST catch this by reference!
   */
  std::unique_ptr<MeshBase> & getMesh(const std::string & param_name,
                                      const bool allow_invalid = false);

  /**
   * Like getMesh(), but for multiple generators.
   *
   * @return The generated meshes
   */
  std::vector<std::unique_ptr<MeshBase> *> getMeshes(const std::string & param_name);

  /**
   * Like \p getMesh(), but takes the name of another MeshGenerator directly.
   *
   * NOTE: You MUST catch this by reference!
   *
   * @return The Mesh generated by that MeshGenerator
   */
  std::unique_ptr<MeshBase> & getMeshByName(const MeshGeneratorName & mesh_generator_name);

  /**
   * Like getMeshByName(), but for multiple generators.
   *
   * @return The generated meshes
   */
  std::vector<std::unique_ptr<MeshBase> *>
  getMeshesByName(const std::vector<MeshGeneratorName> & mesh_generator_names);

  /// References to the mesh and displaced mesh (currently in the ActionWarehouse)
  std::shared_ptr<MooseMesh> & _mesh;

  /// List of selected mesh metadata from the input mesh that need to be retained
  const std::vector<std::string> _selected_mesh_metadata_to_retain;

  /// Whether to retain all the mesh metadata from the input mesh
  const bool _retain_all_input_mesh_metadata;

  /// Names of mesh metadata from the input mesh that need to be forwarded
  std::vector<std::string> _forwarded_metadata_names;
  /// Type indices of mesh metadata from the input mesh that need to be forwarded
  std::vector<type_names_enum> _forwarded_metadata_types;

  /**
   * Build a \p MeshBase object whose underlying type will be determined by the Mesh input file
   * block
   * @param dim The logical dimension of the mesh, e.g. 3 for hexes/tets, 2 for quads/tris. If the
   * caller doesn't specify a value for \p dim, then the value in the \p Mesh input file block will
   * be used
   */
  std::unique_ptr<MeshBase> buildMeshBaseObject(unsigned int dim = libMesh::invalid_uint);

  /**
   * Build a replicated mesh
   * @param dim The logical dimension of the mesh, e.g. 3 for hexes/tets, 2 for quads/tris. If the
   * caller doesn't specify a value for \p dim, then the value in the \p Mesh input file block will
   * be used
   */
  std::unique_ptr<ReplicatedMesh> buildReplicatedMesh(unsigned int dim = libMesh::invalid_uint);

  /**
   * Build a distributed mesh that has correct remote element removal behavior and geometric
   * ghosting functors based on the simulation objects
   * @param dim The logical dimension of the mesh, e.g. 3 for hexes/tets, 2 for quads/tris. If the
   * caller doesn't specify a value for \p dim, then the value in the \p Mesh input file block will
   * be used
   */
  std::unique_ptr<DistributedMesh> buildDistributedMesh(unsigned int dim = libMesh::invalid_uint);

  /**
   * Construct a "subgenerator", a different MeshGenerator subclass
   * that will be added to the same MooseApp on the fly.
   *
   * The new MeshGenerator will be a dependency of this one, so will
   * generate() its mesh first.
   *
   * @return The Mesh generated by the new MeshGenerator subgenerator
   *
   * NOTE: You MUST catch this by reference!
   */
  template <typename... Ts>
  std::unique_ptr<MeshBase> & addMeshSubgenerator(const std::string & generator_name,
                                                  const std::string & name,
                                                  Ts... extra_input_parameters);

  /**
   * Construct a "subgenerator", as above.  User code is responsible
   * for constructing valid InputParameters.
   * @param generator_name the name of the class, also known as the type, of the subgenerator
   * @param name the object name of the subgenerator
   * @param params the input parameters for the subgenerator
   * @return The Mesh generated by the new MeshGenerator subgenerator
   */
  std::unique_ptr<MeshBase> & addMeshSubgenerator(const std::string & generator_name,
                                                  const std::string & name,
                                                  InputParameters & params);

private:
  /// A list of generators that are required to run before this generator may run
  std::vector<std::string> _depends_on;

  /// A nullptr to use for when inputs aren't specified
  std::unique_ptr<MeshBase> _null_mesh = nullptr;
};

template <typename T>
T &
MeshGenerator::declareMeshProperty(const std::string & data_name)
{
  if (_app.executingMeshGenerators())
    mooseError(
        "Declaration of mesh meta data can only happen within the constructor of mesh generators");
  // Check the uniqueness
  if (hasMeshProperty(data_name, _name))
    mooseError("In Mesh Generator ",
               _name,
               ": the to-be-declared mesh metadata named ",
               data_name,
               " has already been declared.");
  // Check if the data type has been included in _type_name_map
  const std::string metadata_type_name = MooseUtils::prettyCppType<T>();
  if (_type_name_map.find(metadata_type_name) == _type_name_map.end())
    mooseError("In Mesh Generator ",
               _name,
               ": the declared mesh metadata named ",
               data_name,
               " has the type, ",
               metadata_type_name,
               ", that has not been included in _type_name_map.");

  std::string full_name =
      std::string(MeshMetaDataInterface::SYSTEM) + "/" + name() + "/" + data_name;

  // Here we will create the RestartableData even though we may not use this instance.
  // If it's already in use, the App will return a reference to the existing instance and we'll
  // return that one instead. We might refactor this to have the app create the RestartableData
  // at a later date.
  auto data_ptr = std::make_unique<RestartableData<T>>(full_name, nullptr);
  auto & restartable_data_ref = static_cast<RestartableData<T> &>(_app.registerRestartableData(
      full_name, std::move(data_ptr), 0, false, MooseApp::MESH_META_DATA));

  return restartable_data_ref.set();
}

template <typename T>
T &
MeshGenerator::declareMeshProperty(const std::string & data_name, const T & init_value)
{
  T & data = declareMeshProperty<T>(data_name);
  data = init_value;

  return data;
}

template <typename T>
inline bool
MeshGenerator::meshMetadataTypeCheck(T /* temp_value */, const std::string type_name)
{
  return MooseUtils::prettyCppType<T>() == type_name;
}

inline MeshGenerator::type_names_enum
MeshGenerator::declareForwardedMeshProperty(const std::string data_name,
                                            const std::string input_mg_name)
{
  const std::string name_old =
      std::string(MeshMetaDataInterface::SYSTEM) + "/" + input_mg_name + "/" + data_name;
  const std::string old_type_str =
      _app.getRestartableMetaData(name_old, MooseApp::MESH_META_DATA, 0).type();

  type_names_enum type_id;
  if (_type_name_map.find(old_type_str) == _type_name_map.end())
    mooseError("In Mesh Generator ",
               _name,
               ": the forwarded mesh metadata named ",
               data_name,
               " has the type, ",
               old_type_str,
               ", that has not been included in _type_name_map.");
  type_id = _type_name_map.find(old_type_str)->second;
  bool is_type_consistent(false);

  switch (type_id)
  {
    case typeVectorUnsignedInt:
      is_type_consistent = meshMetadataTypeCheck(
          declareMeshProperty<std::vector<unsigned int>>(data_name), old_type_str);
      break;
    case typeDouble:
      is_type_consistent =
          meshMetadataTypeCheck(declareMeshProperty<double>(data_name), old_type_str);
      break;
    case typeVectorDouble:
      is_type_consistent =
          meshMetadataTypeCheck(declareMeshProperty<std::vector<double>>(data_name), old_type_str);
      break;
    case typeUnsignedShort:
      is_type_consistent =
          meshMetadataTypeCheck(declareMeshProperty<unsigned short>(data_name), old_type_str);
      break;
    case typeUnsignedInt:
      is_type_consistent =
          meshMetadataTypeCheck(declareMeshProperty<unsigned int>(data_name), old_type_str);
      break;
    case typeBool:
      is_type_consistent =
          meshMetadataTypeCheck(declareMeshProperty<bool>(data_name), old_type_str);
      break;
    case typeUnsignedLongLong:
      is_type_consistent =
          meshMetadataTypeCheck(declareMeshProperty<unsigned long long>(data_name), old_type_str);
      break;
    case typeString:
      is_type_consistent =
          meshMetadataTypeCheck(declareMeshProperty<std::string>(data_name), old_type_str);
    case typeInt:
      is_type_consistent = meshMetadataTypeCheck(declareMeshProperty<int>(data_name), old_type_str);
      break;
    case typePoint:
      is_type_consistent =
          meshMetadataTypeCheck(declareMeshProperty<Point>(data_name), old_type_str);
      break;
    case typeVectorInt:
      is_type_consistent =
          meshMetadataTypeCheck(declareMeshProperty<std::vector<int>>(data_name), old_type_str);
      break;
    case typeVectorUnsignedShort:
      is_type_consistent = meshMetadataTypeCheck(
          declareMeshProperty<std::vector<unsigned short>>(data_name), old_type_str);
      break;
    case typeVectorUnsignedLongLong:
      is_type_consistent = meshMetadataTypeCheck(
          declareMeshProperty<std::vector<unsigned long long>>(data_name), old_type_str);
      break;
    case typeVectorPoint:
      is_type_consistent =
          meshMetadataTypeCheck(declareMeshProperty<std::vector<Point>>(data_name), old_type_str);
      break;
    case typeVectorVectorDouble:
      is_type_consistent = meshMetadataTypeCheck(
          declareMeshProperty<std::vector<std::vector<double>>>(data_name), old_type_str);
      break;
    case typeMapStringPairUnsignedShortUnsignedLongLong:
      is_type_consistent = meshMetadataTypeCheck(
          declareMeshProperty<std::map<std::string, std::pair<unsigned short, unsigned long long>>>(
              data_name),
          old_type_str);
      break;
    case typeShort:
      is_type_consistent =
          meshMetadataTypeCheck(declareMeshProperty<short>(data_name), old_type_str);
      break;
    case typeMapSubdomainIdTypeVectorVectorSubdomainIdType:
      is_type_consistent = meshMetadataTypeCheck(
          declareMeshProperty<
              std::map<subdomain_id_type, std::vector<std::vector<subdomain_id_type>>>>(data_name),
          old_type_str);
      break;
    case typeMapSubdomainIdTypeVectorVectorString:
      is_type_consistent = meshMetadataTypeCheck(
          declareMeshProperty<std::map<subdomain_id_type, std::vector<std::vector<std::string>>>>(
              data_name),
          old_type_str);
      break;
    case typeVectorString:
      is_type_consistent = meshMetadataTypeCheck(
          declareMeshProperty<std::vector<std::string>>(data_name), old_type_str);
      break;
    case typeVectorVectorShort:
      is_type_consistent = meshMetadataTypeCheck(
          declareMeshProperty<std::vector<std::vector<short>>>(data_name), old_type_str);
      break;
    case typeVectorVectorString:
      is_type_consistent = meshMetadataTypeCheck(
          declareMeshProperty<std::vector<std::vector<std::string>>>(data_name), old_type_str);
      break;
    case typeMapBoundaryIDRealVectorValue:
      is_type_consistent = meshMetadataTypeCheck(
          declareMeshProperty<std::map<BoundaryID, RealVectorValue>>(data_name), old_type_str);
      break;
    default:
      mooseError("In Mesh Generator ",
                 _name,
                 ": the forwarded mesh metadata named ",
                 data_name,
                 " has the type, ",
                 old_type_str,
                 ", that has not been included in _type_name_map.");
  }
  if (!is_type_consistent)
    mooseError("In Mesh Generator ",
               _name,
               ": the forwarded mesh metadata named ",
               data_name,
               " was declared using the wrong type name that differs from the expected form, ",
               old_type_str,
               ".");
  return type_id;
}

inline void
MeshGenerator::setForwardedMeshProperty(const std::string data_name,
                                        const std::string input_mg_name,
                                        const type_names_enum type_id)
{
  switch (type_id)
  {
    case typeVectorUnsignedInt:
      setMeshProperty(data_name,
                      getMeshProperty<std::vector<unsigned int>>(data_name, input_mg_name));
      break;
    case typeDouble:
      setMeshProperty(data_name, getMeshProperty<double>(data_name, input_mg_name));
      break;
    case typeVectorDouble:
      setMeshProperty(data_name, getMeshProperty<std::vector<double>>(data_name, input_mg_name));
      break;
    case typeUnsignedShort:
      setMeshProperty(data_name, getMeshProperty<unsigned short>(data_name, input_mg_name));
      break;
    case typeUnsignedInt:
      setMeshProperty(data_name, getMeshProperty<unsigned int>(data_name, input_mg_name));
      break;
    case typeBool:
      setMeshProperty(data_name, getMeshProperty<bool>(data_name, input_mg_name));
      break;
    case typeUnsignedLongLong:
      setMeshProperty(data_name, getMeshProperty<unsigned long long>(data_name, input_mg_name));
      break;
    case typeString:
      setMeshProperty(data_name, getMeshProperty<std::string>(data_name, input_mg_name));
    case typeInt:
      setMeshProperty(data_name, getMeshProperty<int>(data_name, input_mg_name));
      break;
    case typePoint:
      setMeshProperty(data_name, getMeshProperty<Point>(data_name, input_mg_name));
      break;
    case typeVectorInt:
      setMeshProperty(data_name, getMeshProperty<std::vector<int>>(data_name, input_mg_name));
      break;
    case typeVectorUnsignedShort:
      setMeshProperty(data_name,
                      getMeshProperty<std::vector<unsigned short>>(data_name, input_mg_name));
      break;
    case typeVectorUnsignedLongLong:
      setMeshProperty(data_name,
                      getMeshProperty<std::vector<unsigned long long>>(data_name, input_mg_name));
      break;
    case typeVectorPoint:
      setMeshProperty(data_name, getMeshProperty<std::vector<Point>>(data_name, input_mg_name));
      break;
    case typeVectorVectorDouble:
      setMeshProperty(data_name,
                      getMeshProperty<std::vector<std::vector<double>>>(data_name, input_mg_name));
      break;
    case typeMapStringPairUnsignedShortUnsignedLongLong:
      setMeshProperty(
          data_name,
          getMeshProperty<std::map<std::string, std::pair<unsigned short, unsigned long long>>>(
              data_name, input_mg_name));
      break;
    case typeShort:
      setMeshProperty(data_name, getMeshProperty<short>(data_name, input_mg_name));
      break;
    case typeMapSubdomainIdTypeVectorVectorSubdomainIdType:
      setMeshProperty(
          data_name,
          getMeshProperty<std::map<subdomain_id_type, std::vector<std::vector<subdomain_id_type>>>>(
              data_name, input_mg_name));
      break;
    case typeVectorString:
      setMeshProperty(
          data_name,
          getMeshProperty<std::map<subdomain_id_type, std::vector<std::vector<std::string>>>>(
              data_name, input_mg_name));
      break;
    case typeMapSubdomainIdTypeVectorVectorString:
      setMeshProperty(data_name,
                      getMeshProperty<std::vector<std::string>>(data_name, input_mg_name));
      break;
    case typeVectorVectorShort:
      setMeshProperty(data_name,
                      getMeshProperty<std::vector<std::vector<short>>>(data_name, input_mg_name));
      break;
    case typeVectorVectorString:
      setMeshProperty(
          data_name,
          getMeshProperty<std::vector<std::vector<std::string>>>(data_name, input_mg_name));
      break;
    case typeMapBoundaryIDRealVectorValue:
      setMeshProperty(
          data_name,
          getMeshProperty<std::map<BoundaryID, RealVectorValue>>(data_name, input_mg_name));
      break;
    default:
      mooseError("In Mesh Generator ",
                 _name,
                 ": the forwarded mesh metadata named ",
                 data_name,
                 " has the type that has not been included in _type_name_map.");
  }
}

template <typename T>
T &
MeshGenerator::setMeshProperty(const std::string & data_name, const T & data_value)
{
  if (!_app.executingMeshGenerators())
    mooseError("Updating mesh meta data cannot occur in the constructor of mesh generators");

  std::string full_name =
      std::string(MeshMetaDataInterface::SYSTEM) + "/" + name() + "/" + data_name;

  if (_app.getRestartableMetaData(full_name, MooseApp::MESH_META_DATA, 0).type() !=
      MooseUtils::prettyCppType(&data_value))
    mooseError("Data type of metadata value must match the original data type of the metadata");
  auto & restartable_data_ref = dynamic_cast<RestartableData<T> &>(
      _app.getRestartableMetaData(full_name, MooseApp::MESH_META_DATA, 0));
  T & data = restartable_data_ref.set();
  data = data_value;

  return data;
}

template <typename... Ts>
std::unique_ptr<MeshBase> &
MeshGenerator::addMeshSubgenerator(const std::string & generator_name,
                                   const std::string & name,
                                   Ts... extra_input_parameters)
{
  InputParameters subgenerator_params = _app.getFactory().getValidParams(generator_name);

  subgenerator_params.setParameters(extra_input_parameters...);

  return this->addMeshSubgenerator(generator_name, name, subgenerator_params);
}
