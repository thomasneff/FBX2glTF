/**
 * Copyright (c) 2014-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "FbxRoughMetMaterialInfo.hpp"

std::unique_ptr<FbxRoughMetMaterialInfo> FbxRoughMetMaterialInfo::From(
    FbxSurfaceMaterial* fbxMaterial,
    const std::map<const FbxTexture*, FbxString>& textureLocations) {
  std::unique_ptr<FbxRoughMetMaterialInfo> res(
      new FbxRoughMetMaterialInfo(fbxMaterial->GetName(), FBX_SHADER_METROUGH));

  const FbxProperty mayaProp = fbxMaterial->FindProperty("Maya");
  auto getTex = [&](std::string propName) {
    const FbxFileTexture* ptr = nullptr;

    const FbxProperty useProp = mayaProp.FindHierarchical(("use_" + propName + "_map").c_str());
    if (useProp.IsValid() && useProp.Get<bool>()) {
      const FbxProperty texProp = mayaProp.FindHierarchical(("TEX_" + propName + "_map").c_str());
      if (texProp.IsValid()) {
        ptr = texProp.GetSrcObject<FbxFileTexture>();
        if (ptr != nullptr && textureLocations.find(ptr) == textureLocations.end()) {
          ptr = nullptr;
        }
      }
    } else if (verboseOutput && useProp.IsValid()) {
      fmt::printf(
          "Note: Property '%s' of material '%s' exists, but is flagged as 'do not use'.\n",
          propName,
          fbxMaterial->GetName());
    }
    return ptr;
  };

  auto getVec = [&](std::string propName) -> FbxDouble3 {
    const FbxProperty vecProp = mayaProp.FindHierarchical(propName.c_str());
    return vecProp.IsValid() ? vecProp.Get<FbxDouble3>() : FbxDouble3(1, 1, 1);
  };

  auto getVal = [&](std::string propName) -> FbxDouble {
    const FbxProperty vecProp = mayaProp.FindHierarchical(propName.c_str());
    return vecProp.IsValid() ? vecProp.Get<FbxDouble>() : 0;
  };

  if (mayaProp.GetPropertyDataType() != FbxCompoundDT) {
    // return nullptr;

    auto getSurfaceScalar = [&](const char* propName) -> std::tuple<FbxDouble, FbxFileTexture*> {
      const FbxProperty prop = fbxMaterial->FindProperty(propName);

      FbxDouble val(0);
      FbxFileTexture* tex = prop.GetSrcObject<FbxFileTexture>();
      if (tex != nullptr && textureLocations.find(tex) == textureLocations.end()) {
        tex = nullptr;
      }
      if (tex == nullptr && prop.IsValid()) {
        val = prop.Get<FbxDouble>();
      }
      return std::make_tuple(val, tex);
    };

    auto getSurfaceVector = [&](const char* propName) -> std::tuple<FbxDouble3, FbxFileTexture*> {
      const FbxProperty prop = fbxMaterial->FindProperty(propName);

      FbxDouble3 val(1, 1, 1);
      FbxFileTexture* tex = prop.GetSrcObject<FbxFileTexture>();
      if (tex != nullptr && textureLocations.find(tex) == textureLocations.end()) {
        tex = nullptr;
      }
      if (tex == nullptr && prop.IsValid()) {
        val = prop.Get<FbxDouble3>();
      }
      return std::make_tuple(val, tex);
    };

    auto getSurfaceValues =
        [&](const char* colName,
            const char* facName) -> std::tuple<FbxVector4, FbxFileTexture*, FbxFileTexture*> {
      const FbxProperty colProp = fbxMaterial->FindProperty(colName);
      const FbxProperty facProp = fbxMaterial->FindProperty(facName);

      FbxDouble3 colorVal(1, 1, 1);
      FbxDouble factorVal(1);

      FbxFileTexture* colTex = colProp.GetSrcObject<FbxFileTexture>();
      if (colTex != nullptr && textureLocations.find(colTex) == textureLocations.end()) {
        colTex = nullptr;
      }
      if (colTex == nullptr && colProp.IsValid()) {
        colorVal = colProp.Get<FbxDouble3>();
      }
      FbxFileTexture* facTex = facProp.GetSrcObject<FbxFileTexture>();
      if (facTex != nullptr && textureLocations.find(facTex) == textureLocations.end()) {
        facTex = nullptr;
      }
      if (facTex == nullptr && facProp.IsValid()) {
        factorVal = facProp.Get<FbxDouble>();
      }

      auto val = FbxVector4(
          colorVal[0] * factorVal, colorVal[1] * factorVal, colorVal[2] * factorVal, factorVal);
      return std::make_tuple(val, colTex, facTex);
    };

    // four properties are on the same structure and follow the same rules
    auto handleBasicProperty = [&](const char* colName,
                                   const char* facName) -> std::tuple<FbxVector4, FbxFileTexture*> {
      FbxFileTexture *colTex, *facTex;
      FbxVector4 vec;

      std::tie(vec, colTex, facTex) = getSurfaceValues(colName, facName);
      if (colTex) {
        if (facTex) {
          fmt::printf(
              "Warning: Mat [%s]: Can't handle both %s and %s textures; discarding %s.\n",
              "asdf",
              colName,
              facName,
              facName);
        }
        return std::make_tuple(vec, colTex);
      }
      return std::make_tuple(vec, facTex);
    };

    //std::tie(res->colAmbient, res->texAmbient) =
    //    handleBasicProperty(FbxSurfaceMaterial::sAmbient, FbxSurfaceMaterial::sAmbientFactor);
    //std::tie(res->colSpecular, res->texSpecular) =
    //    handleBasicProperty(FbxSurfaceMaterial::sSpecular, FbxSurfaceMaterial::sSpecularFactor);
    std::tie(res->colBase, res->texColor) =
        handleBasicProperty(FbxSurfaceMaterial::sDiffuse, FbxSurfaceMaterial::sDiffuseFactor);

    std::tie(res->colEmissive, res->texEmissive) =
        handleBasicProperty(FbxSurfaceMaterial::sEmissive, FbxSurfaceMaterial::sEmissiveFactor);

    // the normal map can only ever be a map, ignore everything else
    tie(std::ignore, res->texNormal) = getSurfaceVector(FbxSurfaceMaterial::sNormalMap);

	// we take the specular texture and use it as metallic-roughness + occlusion
    tie(std::ignore, res->texMetallic) = getSurfaceVector(FbxSurfaceMaterial::sSpecular);
    res->texRoughness = res->texMetallic;
    res->texAmbientOcclusion = res->texMetallic;
    //res->texNormal = getTex("normal");
    //res->texColor = getTex("color");
    //res->colBase = getVec("base_color");
    //res->texAmbientOcclusion = getTex("ao");
    //res->texEmissive = getTex("emissive");
    //res->colEmissive = getVec("emissive");
    //res->emissiveIntensity = getVal("emissive_intensity");
    //res->texMetallic = getTex("metallic");
    //res->metallic = getVal("metallic");
    //res->texRoughness = getTex("roughness");
    //res->roughness = getVal("roughness");

    return res;
  }
  if (!fbxMaterial->ShadingModel.Get().IsEmpty()) {
    ::fmt::printf(
        "Warning: Material %s has surprising shading model: %s\n",
        fbxMaterial->GetName(),
        fbxMaterial->ShadingModel.Get());
  }

  res->texNormal = getTex("normal");
  res->texColor = getTex("color");
  res->colBase = getVec("base_color");
  res->texAmbientOcclusion = getTex("ao");
  res->texEmissive = getTex("emissive");
  res->colEmissive = getVec("emissive");
  res->emissiveIntensity = getVal("emissive_intensity");
  res->texMetallic = getTex("metallic");
  res->metallic = getVal("metallic");
  res->texRoughness = getTex("roughness");
  res->roughness = getVal("roughness");

  auto test = getTex("specular");

  fmt::printf("HELLO WORLD!");
  return res;
}
