/**
 * =============================================================================
 * DumpSource2
 * Copyright (C) 2024 ValveResourceFormat Contributors
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

#include "filesystem_exporter.h"
#include "globalvariables.h"
#include "interfaces.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <optional>
#include <spdlog/spdlog.h>

namespace Dumpers::Schemas::FilesystemExporter
{

std::string CommentBlock(std::string str)
{
	size_t pos = 0;
	while ((pos = str.find('\n', pos)) != std::string::npos)
	{
		str.replace(pos, 1, "\n//");
		pos += 3;
	}

	return str;
}

void OutputMetadataEntry(const IntermediateMetadata& entry, std::ofstream& output, bool tabulate)
{
	output << (tabulate ? "\t" : "") << "// " << entry.name;

	if (entry.hasValue)
	{
		if (entry.stringValue)
		{
			output << " = " << CommentBlock(*entry.stringValue);
		}
		else
			output << " (UNKNOWN FOR PARSER)";
	}

	output << "\n";
}

void DumpClasses(const std::vector<IntermediateSchemaClass>& classes, std::filesystem::path schemaPath, std::map<std::string, std::unordered_set<std::string>>& foundFiles)
{
	for (const auto& intermediateClass : classes)
	{
		if (!std::filesystem::is_directory(schemaPath / intermediateClass.module))
			if (!std::filesystem::create_directory(schemaPath / intermediateClass.module))
				return;

		// Some classes have :: in them which we can't save.
		auto sanitizedFileName = intermediateClass.name;
		std::replace(sanitizedFileName.begin(), sanitizedFileName.end(), ':', '_');

		// We save the file in a map so that we know which files are outdated and should be removed
		foundFiles[intermediateClass.module].insert(sanitizedFileName);

		std::ofstream output((schemaPath / intermediateClass.module / sanitizedFileName).replace_extension(".h"));

		// Output metadata entries as comments before the class definition
		spdlog::trace("Dumping class: '{}'", intermediateClass.name);
		for (const auto& metadata : intermediateClass.metadata)
		{
			OutputMetadataEntry(metadata, output, false);
		}

		output << "class " << intermediateClass.name;
		Globals::stringsIgnoreStream << intermediateClass.name << "\n";

		bool wroteBase = false;
		for (const auto& parent : intermediateClass.parents)
		{
			if (!wroteBase)
			{
				output << " : public " << parent.name;
				wroteBase = true;
			}
			else
			{
				output << ", public " << parent.name;
			}
		}

		output << "\n{\n";

		for (const auto& field : intermediateClass.fields)
		{
			spdlog::trace("Dumping field: '{}' for class: '{}'", field.name, intermediateClass.name);
			// Output metadata entires as comments before the field definition
			for (const auto& metadata : field.metadata)
			{
				OutputMetadataEntry(metadata, output, true);
			}

			output << "\t" << field.type->m_sTypeName.String() << " " << field.name << ";\n";
			Globals::stringsIgnoreStream << field.name << "\n";
		}

		output << "};\n";
	}
}

void DumpEnums(const std::vector<IntermediateSchemaEnum>& enums, std::filesystem::path schemaPath, std::map<std::string, std::unordered_set<std::string>>& foundFiles)
{
	for (const auto& intermediateEnum : enums)
	{
		if (!std::filesystem::is_directory(schemaPath / intermediateEnum.module))
			if (!std::filesystem::create_directory(schemaPath / intermediateEnum.module))
				return;

		// Some classes have :: in them which we can't save.
		auto sanitizedFileName = intermediateEnum.name;
		std::replace(sanitizedFileName.begin(), sanitizedFileName.end(), ':', '_');

		// We save the file in a map so that we know which files are outdated and should be removed
		foundFiles[intermediateEnum.module].insert(sanitizedFileName);

		std::ofstream output((schemaPath / intermediateEnum.module / sanitizedFileName).replace_extension(".h"));

		for (const auto& metadata : intermediateEnum.metadata)
		{
			OutputMetadataEntry(metadata, output, false);
		}

		output << "enum " << intermediateEnum.name << " : " << (intermediateEnum.stringAlignment.has_value() ? *intermediateEnum.stringAlignment : "unknown alignment type") << "\n{\n";
		Globals::stringsIgnoreStream << intermediateEnum.name << "\n";

		for (const auto& member : intermediateEnum.members)
		{
			// Output metadata entires as comments before the field definition
			for (const auto& metadata : member.metadata)
			{
				OutputMetadataEntry(metadata, output, true);
			}

			output << "\t" << member.name << " = " << member.value << ",\n";
			Globals::stringsIgnoreStream << member.name << "\n";
		}

		output << "};\n";
	}
}

void Dump(const std::vector<IntermediateSchemaEnum>& enums, const std::vector<IntermediateSchemaClass>& classes)
{
	spdlog::info("Dumping schemas to filesystem");
	const auto schemaPath = Globals::outputPath / "schemas";

	if (!std::filesystem::is_directory(schemaPath))
		if (!std::filesystem::create_directory(schemaPath))
			return;

	std::map<std::string, std::unordered_set<std::string>> foundFiles;

	DumpClasses(classes, schemaPath, foundFiles);
	DumpEnums(enums, schemaPath, foundFiles);

	for (const auto& entry : std::filesystem::directory_iterator(schemaPath))
	{
		auto projectName = entry.path().filename().string();
		bool isInMap = foundFiles.find(projectName) != foundFiles.end();

		if (entry.is_directory() && !isInMap)
		{
			spdlog::info("Removing orphan schema folder {}", entry.path().generic_string());
			std::filesystem::remove_all(entry.path());
		}
		else if (isInMap)
		{

			for (const auto& typeScopePath : std::filesystem::directory_iterator(entry.path()))
			{
				auto& filesSet = foundFiles[projectName];

				if (filesSet.find(typeScopePath.path().stem().string()) == filesSet.end())
				{
					spdlog::info("Removing orphan schema file {}", typeScopePath.path().generic_string());
					std::filesystem::remove(typeScopePath.path());
				}
			}
		}
	}
}

} // namespace Dumpers::Schemas::FilesystemExporter