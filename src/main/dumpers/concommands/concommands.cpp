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


// Yes this is shit, no we can't make it better.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#include <icvar.h>
#undef private
#undef _ALLOW_KEYWORD_MACROS

#include "concommands.h"
#include "interfaces.h"
#include "globalvariables.h"
#include <algorithm>
#include <fstream>
#include <vector>
#include <iostream>
#include <spdlog/spdlog.h>

namespace Dumpers::ConCommands
{

#define MINMAXVALUEPRINT(typeName) \
	stream << " " << value->typeName; 												\
																														\
	bool hasMinValue = cvar.HasMin();													\
	bool hasMaxValue = cvar.HasMax();													\
																														\
	stream << " (";																						\
																														\
	if (hasMinValue)																					\
		stream << "min: " << conVarData->MinValue()->typeName;	\
																														\
	if (hasMaxValue)																					\
	{																													\
		if (hasMinValue)																				\
			stream << ", ";																				\
		stream << "max: " << conVarData->MaxValue()->typeName;		\
	}																													\
																														\
	if (hasMinValue || hasMaxValue)														\
		stream << ", ";																					\
																														\
	WriteFlags(cvar.GetFlags(), stream);											\
																														\
	stream << ")";

#define FCVAR_MISSING1	(1ull<<30)
#define FCVAR_MISSING2	(1ull<<31)

std::vector<std::pair<uint64_t, const char*>> g_flagMap{
	{FCVAR_LINKED_CONCOMMAND, "linked_concommand"},
	{FCVAR_DEVELOPMENTONLY, "developmentonly"},
	{FCVAR_GAMEDLL, "gamedll"},
	{FCVAR_CLIENTDLL, "clientdll"},
	{FCVAR_HIDDEN, "hidden"},
	{FCVAR_PROTECTED, "protected"},
	{FCVAR_SPONLY, "sponly"},
	{FCVAR_ARCHIVE, "archive"},
	{FCVAR_NOTIFY, "notify"},
	{FCVAR_USERINFO, "userinfo"},
	{FCVAR_REFERENCE, "reference"},
	{FCVAR_UNLOGGED, "unlogged"},
	{FCVAR_INITIAL_SETVALUE, "initial_setvalue"},
	{FCVAR_REPLICATED, "replicated"},
	{FCVAR_CHEAT, "cheat"},
	{FCVAR_PER_USER, "per_user"},
	{FCVAR_DEMO, "demo"},
	{FCVAR_DONTRECORD, "dontrecord"},
	{FCVAR_PERFORMING_CALLBACKS, "performing_Callbacks"},
	{FCVAR_RELEASE, "release"},
	{FCVAR_MENUBAR_ITEM, "menubar_item"},
	{FCVAR_COMMANDLINE_ENFORCED, "commandline_enforced"},
	{FCVAR_NOT_CONNECTED, "notconnected"},
	{FCVAR_VCONSOLE_FUZZY_MATCHING, "vconsole_fuzzy_matching"},
	{FCVAR_SERVER_CAN_EXECUTE, "server_can_execute"},
	{FCVAR_CLIENT_CAN_EXECUTE, "client_can_execute"},
	{FCVAR_SERVER_CANNOT_QUERY, "server_cannot_query"},
	{FCVAR_VCONSOLE_SET_FOCUS, "vconsole_set_focus"},
	{FCVAR_CLIENTCMD_CAN_EXECUTE, "clientcmd_can_execute"},
	{FCVAR_EXECUTE_PER_TICK, "execute_per_tick"},
	{FCVAR_MISSING1, "missing1"},
	{FCVAR_MISSING2, "missing2"},
	{FCVAR_DEFENSIVE, "defensive"}
};

void WriteFlags(uint64_t flags, std::ofstream& stream)
{
	bool found = false;
	for (const auto& [value, name] : g_flagMap)
	{
		if (flags & value)
		{
			stream << (found ? " " : "") << name;
			found = true;
		}
	}
}
void WriteValueLine(ConVarRefAbstract cvar, std::ofstream& stream)
{
	auto conVarData = cvar.GetConVarData();
	auto value = cvar.GetConVarData()->DefaultValue();

	switch (cvar.GetType())
	{
	case EConVarType_Bool:
	{
		stream << " " << (value->m_bValue ? "true" : "false") << " (";
		WriteFlags(cvar.GetFlags(), stream);
		stream << ")";
		break;
	}
	case EConVarType_Int16:
	{
		MINMAXVALUEPRINT(m_i16Value);
		break;
	}
	case EConVarType_Int32:
	{
		MINMAXVALUEPRINT(m_i32Value);
		break;
	}
	case EConVarType_UInt32:
	{
		MINMAXVALUEPRINT(m_u32Value);
		break;
	}
	case EConVarType_Int64:
	{
		MINMAXVALUEPRINT(m_i64Value);
		break;
	}
	case EConVarType_UInt64:
	{
		MINMAXVALUEPRINT(m_u64Value);
		break;
	}
	case EConVarType_Float32:
	{
		MINMAXVALUEPRINT(m_fl32Value);
		break;
	}
	case EConVarType_Float64:
	{
		MINMAXVALUEPRINT(m_fl64Value);
		break;
	}
	case EConVarType_String:
	{
		stream << " \"" << (value->m_StringValue.m_pString ? value->m_StringValue.m_pString : "") << "\"" << " (";
		WriteFlags(cvar.GetFlags(), stream);
		stream << ")";
		break;
	}
	case EConVarType_Color:
	{
		stream << " [" << value->m_clrValue.r() << ", " << value->m_clrValue.g() << ", " << value->m_clrValue.b() << ", " << value->m_clrValue.a() << "]" << " (";
		WriteFlags(cvar.GetFlags(), stream);
		stream << ")";
		break;
	}
	case EConVarType_Vector2:
	{
		stream << " [" << value->m_vec2Value.x << ", " << value->m_vec2Value.y << "]" << " (";
		WriteFlags(cvar.GetFlags(), stream);
		stream << ")";
		break;
	}
	case EConVarType_Vector3:
	{
		stream << " [" << value->m_vec3Value.x << ", " << value->m_vec3Value.y << ", " << value->m_vec3Value.z << "]" << " (";
		WriteFlags(cvar.GetFlags(), stream);
		stream << ")";
		break;
	}
	case EConVarType_Vector4:
	{
		stream << " [" << value->m_vec4Value.x << ", " << value->m_vec4Value.y << ", " << value->m_vec4Value.z << ", " << value->m_vec4Value.w << "]" << " (";
		WriteFlags(cvar.GetFlags(), stream);
		stream << ")";
		break;
	}
	case EConVarType_Qangle:
	{
		stream << " [" << value->m_angValue.x << ", " << value->m_angValue.y << ", " << value->m_angValue.z << "]" << " (";
		WriteFlags(cvar.GetFlags(), stream);
		stream << ")";
		break;
	}
	default:
		stream << " UNKNOWN VALUE TYPE";
		break;
	}
}

void FixNewlineTabbing(std::string& str)
{
	auto it = str.begin();
	while ((it = std::find(it, str.end(), '\n')) != str.end())
	{
		if (it + 1 == str.end() || *(it + 1) != '\t')
			it = str.insert(it + 1, '\t') + 1;
		else
			it++;
	}

	// trim end of string
	if (str.back() == '\t')
		str.pop_back();

	if (str.back() == '\n')
		str.pop_back();
}

std::string EscapeDescription(std::string str)
{
	for (auto it = str.begin(); it != str.end(); it++) {
		if (*it == '\n')
		{
			*it = '\\';
			it = str.insert(it + 1, 'n');
		}
		else if (*it == '\t')
		{
			*it = '\\';
			it = str.insert(it + 1, 't');
		}
	}

	return str;
}


void DumpConVars()
{
	spdlog::info("Dumping convars");
	std::vector<ConVarRefAbstract> convars;
	// there's always gonna be a lot of convars, let's save some reallocations
	convars.reserve(1000);

	for (ConVarRefAbstract ref(ConVarRef((uint16)0)); ref.IsValidRef(); ref = ConVarRefAbstract(ConVarRef(ref.GetAccessIndex() + 1)))
	{
		convars.push_back(ref);
	}

	std::sort(convars.begin(), convars.end(), [](const ConVarRefAbstract a, const ConVarRefAbstract b) {
		return strcmp(a.GetName(), b.GetName()) < 0;
	});

	std::ofstream output(Globals::outputPath / "convars.txt");

	for (const auto cvar : convars)
	{
		std::string helpString = "<no description>";
		if (cvar.HasHelpText())
		{
			helpString = cvar.GetHelpText();
			Globals::stringsIgnoreStream << EscapeDescription(helpString) << "\n";
			FixNewlineTabbing(helpString);
		}

		output << cvar.GetName();
		WriteValueLine(cvar, output);
		output << "\n\t" << helpString;
		output << "\n" << std::endl;

		Globals::stringsIgnoreStream << cvar.GetName() << "\n";
	}

	output.close();
}

void DumpCommands()
{
	spdlog::info("Dumping commands");
	std::vector<ConCommandRef> commands;
	// there's always gonna be a lot of commands, let's save some reallocations
	commands.reserve(1000);
	ConCommandData* data = Interfaces::cvar->GetConCommandData(ConCommandRef());
	for (ConCommandRef ref = ConCommandRef((uint16)0); ref.GetRawData() != data; ref = ConCommandRef(ref.GetAccessIndex() + 1))
	{
		commands.push_back(ref);
	}

	std::sort(commands.begin(), commands.end(), [](const ConCommandRef a, const ConCommandRef b) {
		return strcmp(a.GetName(), b.GetName()) < 0;
	});

	std::ofstream output(Globals::outputPath / "commands.txt");

	for (const auto command : commands)
	{
		std::string helpString = "<no description>";
		if (command.HasHelpText())
		{
			helpString = command.GetHelpText();
			Globals::stringsIgnoreStream << EscapeDescription(helpString) << "\n";
			FixNewlineTabbing(helpString);
		}

		output << command.GetName() << " (";
		WriteFlags(command.GetFlags(), output);
		output << ")\n\t" << helpString;
		output << "\n" << std::endl;

		Globals::stringsIgnoreStream << command.GetName() << "\n";
	}

	output.close();
}

void Dump()
{
	spdlog::debug("Removing default value from cl_color");
	// cl_color has a random default value on each start.
	if (ConVarRefAbstract cvar("cl_color"); cvar.IsValidRef())
	{
		cvar.GetConVarData()->RemoveDefaultValue();
	}

	DumpConVars();
	DumpCommands();
}

} // namespace Dumpers::ConCommands