/**
 * =============================================================================
 * DumpSource2
 * Copyright (C) 2026 ValveResourceFormat Contributors
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "json_exporter.h"
#include "globalvariables.h"
#include "interfaces.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <optional>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Dumpers::Schemas::JsonExporter
{

json SerializeMetadataArray(const std::vector<IntermediateMetadata>& metadataVector)
{
	json arr = json::array();
	for (const auto& metadata : metadataVector)
	{
		json j;
		j["name"] = metadata.name;
		if (metadata.hasValue && metadata.stringValue.has_value())
		{
			j["value"] = *metadata.stringValue;
		}

		arr.push_back(j);
	}
	return arr;
}

json SerializeType(CSchemaType* type)
{
	if (!type)
		return nullptr;

	json j;

	switch (type->m_eTypeCategory)
	{
	case SCHEMA_TYPE_BUILTIN:
		j["category"] = "builtin";
		j["name"] = type->m_sTypeName.String();
		break;
	case SCHEMA_TYPE_POINTER:
		j["category"] = "ptr";
		j["inner"] = SerializeType(static_cast<CSchemaType_Ptr*>(type)->m_pObjectType);
		break;
	case SCHEMA_TYPE_FIXED_ARRAY:
	{
		auto* arr = static_cast<CSchemaType_FixedArray*>(type);
		j["category"] = "fixed_array";
		j["count"] = arr->m_nElementCount;
		j["inner"] = SerializeType(arr->m_pElementType);
		break;
	}
	case SCHEMA_TYPE_ATOMIC:
	{
		j["category"] = "atomic";

		// Extract outer name from m_sTypeName before '<'
		std::string typeName = type->m_sTypeName.String();
		auto pos = typeName.find('<');
		if (pos != std::string::npos)
		{
			auto name = typeName.substr(0, pos);
			// Trim trailing whitespace
			while (!name.empty() && name.back() == ' ')
				name.pop_back();
			j["name"] = name;
		}
		else
		{
			j["name"] = typeName;
		}

		if (type->m_eAtomicCategory == SCHEMA_ATOMIC_T ||
			type->m_eAtomicCategory == SCHEMA_ATOMIC_COLLECTION_OF_T)
		{
			j["inner"] = SerializeType(static_cast<CSchemaType_Atomic_T*>(type)->m_pTemplateType);
		}
		else if (type->m_eAtomicCategory == SCHEMA_ATOMIC_TT)
		{
			auto* tt = static_cast<CSchemaType_Atomic_TT*>(type);
			j["inner"] = SerializeType(tt->m_pTemplateType);
			j["inner2"] = SerializeType(tt->m_pTemplateType2);
		}
		break;
	}
	case SCHEMA_TYPE_DECLARED_CLASS:
	{
		j["category"] = "declared_class";
		auto* classType = static_cast<CSchemaType_DeclaredClass*>(type);
		if (classType->m_pClassInfo && classType->m_pClassInfo->m_pszName)
		{
			j["name"] = classType->m_pClassInfo->m_pszName;
			j["module"] = classType->m_pClassInfo->m_pszProjectName;
		}
		else
			j["name"] = type->m_sTypeName.String();
		break;
	}
	case SCHEMA_TYPE_DECLARED_ENUM:
	{
		j["category"] = "declared_enum";
		auto* enumType = static_cast<CSchemaType_DeclaredEnum*>(type);
		if (enumType->m_pEnumInfo && enumType->m_pEnumInfo->m_pszName)
		{
			j["name"] = enumType->m_pEnumInfo->m_pszName;
			j["module"] = enumType->m_pEnumInfo->m_pszProjectName;
		}
		else
			j["name"] = type->m_sTypeName.String();
		break;
	}
	case SCHEMA_TYPE_BITFIELD:
		j["category"] = "bitfield";
		j["count"] = static_cast<CSchemaType_Bitfield*>(type)->m_nBitfieldCount;
		break;
	default:
		j["category"] = "builtin";
		j["name"] = type->m_sTypeName.String();
		break;
	}

	return j;
}

void DumpClasses(const std::vector<IntermediateSchemaClass>& classes, json& classesArray)
{
	for (const auto& intermediateClass : classes)
	{
		spdlog::trace("Dumping class for json: '{}'", intermediateClass.name);

		json classObj;
		classObj["name"] = intermediateClass.name;
		classObj["module"] = intermediateClass.module;
		classObj["metadata"] = SerializeMetadataArray(intermediateClass.metadata);

		json parents = json::array();
		for (const auto& parent : intermediateClass.parents)
		{
			json parentObj;
			parentObj["name"] = parent.name;
			parentObj["module"] = parent.module;
			parents.push_back(std::move(parentObj));
		}

		classObj["parents"] = parents;

		json fields = json::array();
		for (const auto& field : intermediateClass.fields)
		{
			spdlog::trace("Dumping field: '{}' for class: '{}'", field.name, intermediateClass.name);
			json fieldObj;
			fieldObj["name"] = field.name;
			fieldObj["offset"] = field.offset;
			fieldObj["type"] = SerializeType(field.type);
			fieldObj["metadata"] = SerializeMetadataArray(field.metadata);
			fields.push_back(fieldObj);
		}
		classObj["fields"] = fields;

		classesArray.push_back(std::move(classObj));
	}
}
	
void DumpEnums(const std::vector<IntermediateSchemaEnum>& enums, json& enumsArray)
{
	for (const auto& intermediateEnum : enums)
	{
		json enumObj;
		enumObj["name"] = intermediateEnum.name;
		enumObj["module"] = intermediateEnum.module;
		enumObj["alignment"] = intermediateEnum.stringAlignment;

		enumObj["metadata"] = SerializeMetadataArray(intermediateEnum.metadata);

		json members = json::array();
		for (const auto& member : intermediateEnum.members)
		{
			json memberObj;
			memberObj["name"] = member.name;
			memberObj["value"] = member.value;

			memberObj["metadata"] = SerializeMetadataArray(member.metadata);
			members.push_back(std::move(memberObj));
		}
		enumObj["members"] = std::move(members);
		enumsArray.push_back(std::move(enumObj));
	}
}

void Dump(const std::vector<IntermediateSchemaEnum>& enums, const std::vector<IntermediateSchemaClass>& classes)
{
	spdlog::info("Dumping schemas to json");

	json root;
	json classesArray = json::array();
	json enumsArray = json::array();

	DumpClasses(classes, classesArray);
	DumpEnums(enums, enumsArray);

	root["classes"] = classesArray;
	root["enums"] = enumsArray;

	std::ofstream output(Globals::outputPath / "schemas.json");
	output << root.dump(1);

	spdlog::info("Wrote schemas.json ({} classes, {} enums)", classesArray.size(), enumsArray.size());
}

} // namespace Dumpers::Schemas::JsonExporter