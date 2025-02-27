/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/log.h>

#include <engine/config.h>
#include <engine/shared/config.h>
#include <engine/shared/console.h>
#include <engine/shared/protocol.h>
#include <engine/storage.h>

CConfig g_Config;

static void EscapeParam(char *pDst, const char *pSrc, int Size)
{
	str_escape(&pDst, pSrc, pDst + Size);
}

struct SConfigVariable
{
	enum EVariableType
	{
		VAR_INT,
		VAR_COLOR,
		VAR_STRING,
	};
	IConsole *m_pConsole;
	const char *m_pScriptName;
	EVariableType m_Type;
	int m_Flags;
	const char *m_pHelp;
	// Note that this only applies to the console command and the SetValue function,
	// but the underlying config variable can still be modified programatically.
	bool m_ReadOnly = false;

	SConfigVariable(IConsole *pConsole, const char *pScriptName, EVariableType Type, int Flags, const char *pHelp) :
		m_pConsole(pConsole),
		m_pScriptName(pScriptName),
		m_Type(Type),
		m_Flags(Flags),
		m_pHelp(pHelp)
	{
	}

	virtual ~SConfigVariable() = default;

	virtual void Register() = 0;
	virtual bool IsDefault() const = 0;
	virtual void Serialize(char *pOut, size_t Size) const = 0;
	virtual void ResetToDefault() = 0;
	virtual void ResetToOld() = 0;

protected:
	void ExecuteLine(const char *pLine)
	{
		m_pConsole->ExecuteLine(pLine, (m_Flags & CFGFLAG_GAME) != 0 ? IConsole::CLIENT_ID_GAME : -1);
	}

	bool CheckReadOnly()
	{
		if(!m_ReadOnly)
			return false;
		char aBuf[IConsole::CMDLINE_LENGTH + 64];
		str_format(aBuf, sizeof(aBuf), "The config variable '%s' cannot be changed right now.", m_pScriptName);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
		return true;
	}
};

struct SIntConfigVariable : public SConfigVariable
{
	int *m_pVariable;
	int m_Default;
	int m_Min;
	int m_Max;
	int m_OldValue;

	SIntConfigVariable(IConsole *pConsole, const char *pScriptName, EVariableType Type, int Flags, const char *pHelp, int *pVariable, int Default, int Min, int Max) :
		SConfigVariable(pConsole, pScriptName, Type, Flags, pHelp),
		m_pVariable(pVariable),
		m_Default(Default),
		m_Min(Min),
		m_Max(Max),
		m_OldValue(Default)
	{
		*m_pVariable = m_Default;
	}

	~SIntConfigVariable() override = default;

	static void CommandCallback(IConsole::IResult *pResult, void *pUserData)
	{
		SIntConfigVariable *pData = static_cast<SIntConfigVariable *>(pUserData);

		if(pResult->NumArguments())
		{
			if(pData->CheckReadOnly())
				return;

			int Value = pResult->GetInteger(0);

			// do clamping
			if(pData->m_Min != pData->m_Max)
			{
				if(Value < pData->m_Min)
					Value = pData->m_Min;
				if(pData->m_Max != 0 && Value > pData->m_Max)
					Value = pData->m_Max;
			}

			*pData->m_pVariable = Value;
			if(pResult->m_ClientID != IConsole::CLIENT_ID_GAME)
				pData->m_OldValue = Value;
		}
		else
		{
			char aBuf[32];
			str_format(aBuf, sizeof(aBuf), "Value: %d", *pData->m_pVariable);
			pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
		}
	}

	void Register() override
	{
		m_pConsole->Register(m_pScriptName, "?i", m_Flags, CommandCallback, this, m_pHelp);
	}

	bool IsDefault() const override
	{
		return *m_pVariable == m_Default;
	}

	void Serialize(char *pOut, size_t Size, int Value) const
	{
		str_format(pOut, Size, "%s %i", m_pScriptName, Value);
	}

	void Serialize(char *pOut, size_t Size) const override
	{
		Serialize(pOut, Size, *m_pVariable);
	}

	void SetValue(int Value)
	{
		if(CheckReadOnly())
			return;
		char aBuf[IConsole::CMDLINE_LENGTH];
		Serialize(aBuf, sizeof(aBuf), Value);
		ExecuteLine(aBuf);
	}

	void ResetToDefault() override
	{
		SetValue(m_Default);
	}

	void ResetToOld() override
	{
		SetValue(m_OldValue);
	}
};

struct SColorConfigVariable : public SConfigVariable
{
	unsigned *m_pVariable;
	unsigned m_Default;
	bool m_Light;
	bool m_Alpha;
	unsigned m_OldValue;

	SColorConfigVariable(IConsole *pConsole, const char *pScriptName, EVariableType Type, int Flags, const char *pHelp, unsigned *pVariable, unsigned Default) :
		SConfigVariable(pConsole, pScriptName, Type, Flags, pHelp),
		m_pVariable(pVariable),
		m_Default(Default),
		m_Light(Flags & CFGFLAG_COLLIGHT),
		m_Alpha(Flags & CFGFLAG_COLALPHA),
		m_OldValue(Default)
	{
		*m_pVariable = m_Default;
	}

	~SColorConfigVariable() override = default;

	static void CommandCallback(IConsole::IResult *pResult, void *pUserData)
	{
		SColorConfigVariable *pData = static_cast<SColorConfigVariable *>(pUserData);

		if(pResult->NumArguments())
		{
			if(pData->CheckReadOnly())
				return;

			const ColorHSLA Color = pResult->GetColor(0, pData->m_Light);
			const unsigned Value = Color.Pack(pData->m_Light ? 0.5f : 0.0f, pData->m_Alpha);

			*pData->m_pVariable = Value;
			if(pResult->m_ClientID != IConsole::CLIENT_ID_GAME)
				pData->m_OldValue = Value;
		}
		else
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "Value: %u", *pData->m_pVariable);
			pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);

			ColorHSLA Hsla = ColorHSLA(*pData->m_pVariable, true);
			if(pData->m_Light)
				Hsla = Hsla.UnclampLighting();
			str_format(aBuf, sizeof(aBuf), "H: %d°, S: %d%%, L: %d%%", round_truncate(Hsla.h * 360), round_truncate(Hsla.s * 100), round_truncate(Hsla.l * 100));
			pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);

			const ColorRGBA Rgba = color_cast<ColorRGBA>(Hsla);
			str_format(aBuf, sizeof(aBuf), "R: %d, G: %d, B: %d, #%06X", round_truncate(Rgba.r * 255), round_truncate(Rgba.g * 255), round_truncate(Rgba.b * 255), Rgba.Pack(false));
			pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);

			if(pData->m_Alpha)
			{
				str_format(aBuf, sizeof(aBuf), "A: %d%%", round_truncate(Hsla.a * 100));
				pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
			}
		}
	}

	void Register() override
	{
		m_pConsole->Register(m_pScriptName, "?i", m_Flags, CommandCallback, this, m_pHelp);
	}

	bool IsDefault() const override
	{
		return *m_pVariable == m_Default;
	}

	void Serialize(char *pOut, size_t Size, unsigned Value) const
	{
		str_format(pOut, Size, "%s %u", m_pScriptName, Value);
	}

	void Serialize(char *pOut, size_t Size) const override
	{
		Serialize(pOut, Size, *m_pVariable);
	}

	void SetValue(unsigned Value)
	{
		if(CheckReadOnly())
			return;
		char aBuf[IConsole::CMDLINE_LENGTH];
		Serialize(aBuf, sizeof(aBuf), Value);
		ExecuteLine(aBuf);
	}

	void ResetToDefault() override
	{
		SetValue(m_Default);
	}

	void ResetToOld() override
	{
		SetValue(m_OldValue);
	}
};

struct SStringConfigVariable : public SConfigVariable
{
	char *m_pStr;
	const char *m_pDefault;
	size_t m_MaxSize;
	char *m_pOldValue;

	SStringConfigVariable(IConsole *pConsole, const char *pScriptName, EVariableType Type, int Flags, const char *pHelp, char *pStr, const char *pDefault, size_t MaxSize, char *pOldValue) :
		SConfigVariable(pConsole, pScriptName, Type, Flags, pHelp),
		m_pStr(pStr),
		m_pDefault(pDefault),
		m_MaxSize(MaxSize),
		m_pOldValue(pOldValue)
	{
		str_copy(m_pStr, m_pDefault, m_MaxSize);
		str_copy(m_pOldValue, m_pDefault, m_MaxSize);
	}

	~SStringConfigVariable() override = default;

	static void CommandCallback(IConsole::IResult *pResult, void *pUserData)
	{
		SStringConfigVariable *pData = static_cast<SStringConfigVariable *>(pUserData);

		if(pResult->NumArguments())
		{
			if(pData->CheckReadOnly())
				return;

			const char *pString = pResult->GetString(0);
			if(!str_utf8_check(pString))
			{
				char aTemp[4];
				size_t Length = 0;
				while(*pString)
				{
					size_t Size = str_utf8_encode(aTemp, static_cast<unsigned char>(*pString++));
					if(Length + Size < pData->m_MaxSize)
					{
						mem_copy(pData->m_pStr + Length, aTemp, Size);
						Length += Size;
					}
					else
						break;
				}
				pData->m_pStr[Length] = '\0';
			}
			else
				str_copy(pData->m_pStr, pString, pData->m_MaxSize);

			if(pResult->m_ClientID != IConsole::CLIENT_ID_GAME)
				str_copy(pData->m_pOldValue, pData->m_pStr, pData->m_MaxSize);
		}
		else
		{
			char aBuf[1024];
			str_format(aBuf, sizeof(aBuf), "Value: %s", pData->m_pStr);
			pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
		}
	}

	void Register() override
	{
		m_pConsole->Register(m_pScriptName, "?r", m_Flags, CommandCallback, this, m_pHelp);
	}

	bool IsDefault() const override
	{
		return str_comp(m_pStr, m_pDefault) == 0;
	}

	void Serialize(char *pOut, size_t Size, const char *pValue) const
	{
		str_copy(pOut, m_pScriptName, Size);
		str_append(pOut, " \"", Size);
		const int OutLen = str_length(pOut);
		EscapeParam(pOut + OutLen, pValue, Size - OutLen - 1); // -1 to ensure space for final quote
		str_append(pOut, "\"", Size);
	}

	void Serialize(char *pOut, size_t Size) const override
	{
		Serialize(pOut, Size, m_pStr);
	}

	void SetValue(const char *pValue)
	{
		if(CheckReadOnly())
			return;
		char aBuf[2048];
		Serialize(aBuf, sizeof(aBuf), pValue);
		ExecuteLine(aBuf);
	}

	void ResetToDefault() override
	{
		SetValue(m_pDefault);
	}

	void ResetToOld() override
	{
		SetValue(m_pOldValue);
	}
};

CConfigManager::CConfigManager()
{
	m_pConsole = nullptr;
	m_pStorage = nullptr;
	m_ConfigFile = 0;
	m_Failed = false;
}

void CConfigManager::Init()
{
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	const auto &&AddVariable = [this](SConfigVariable *pVariable) {
		m_vpAllVariables.push_back(pVariable);
		if((pVariable->m_Flags & CFGFLAG_GAME) != 0)
			m_vpGameVariables.push_back(pVariable);
		pVariable->Register();
	};

#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) \
	{ \
		const char *pHelp = Min == Max ? Desc " (default: " #Def ")" : Max == 0 ? Desc " (default: " #Def ", min: " #Min ")" : Desc " (default: " #Def ", min: " #Min ", max: " #Max ")"; \
		AddVariable(m_ConfigHeap.Allocate<SIntConfigVariable>(m_pConsole, #ScriptName, SConfigVariable::VAR_INT, Flags, pHelp, &g_Config.m_##Name, Def, Min, Max)); \
	}

#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) \
	{ \
		const size_t HelpSize = (size_t)str_length(Desc) + 32; \
		char *pHelp = static_cast<char *>(m_ConfigHeap.Allocate(HelpSize)); \
		const bool Alpha = ((Flags)&CFGFLAG_COLALPHA) != 0; \
		str_format(pHelp, HelpSize, "%s (default: $%0*X)", Desc, Alpha ? 8 : 6, color_cast<ColorRGBA>(ColorHSLA(Def, Alpha)).Pack(Alpha)); \
		AddVariable(m_ConfigHeap.Allocate<SColorConfigVariable>(m_pConsole, #ScriptName, SConfigVariable::VAR_COLOR, Flags, pHelp, &g_Config.m_##Name, Def)); \
	}

#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) \
	{ \
		const size_t HelpSize = (size_t)str_length(Desc) + str_length(Def) + 64; \
		char *pHelp = static_cast<char *>(m_ConfigHeap.Allocate(HelpSize)); \
		str_format(pHelp, HelpSize, "%s (default: \"%s\", max length: %d)", Desc, Def, Len - 1); \
		char *pOldValue = static_cast<char *>(m_ConfigHeap.Allocate(Len)); \
		AddVariable(m_ConfigHeap.Allocate<SStringConfigVariable>(m_pConsole, #ScriptName, SConfigVariable::VAR_STRING, Flags, pHelp, g_Config.m_##Name, Def, Len, pOldValue)); \
	}

#include "config_variables.h"

#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR

	m_pConsole->Register("reset", "s[config-name]", CFGFLAG_SERVER | CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Reset, this, "Reset a config to its default value");
	m_pConsole->Register("toggle", "s[config-option] i[value 1] i[value 2]", CFGFLAG_SERVER | CFGFLAG_CLIENT, Con_Toggle, this, "Toggle config value");
	m_pConsole->Register("+toggle", "s[config-option] i[value 1] i[value 2]", CFGFLAG_CLIENT, Con_ToggleStroke, this, "Toggle config value via keypress");
}

void CConfigManager::Reset(const char *pScriptName)
{
	for(SConfigVariable *pVariable : m_vpAllVariables)
	{
		if((pVariable->m_Flags & m_pConsole->FlagMask()) != 0 && str_comp(pScriptName, pVariable->m_pScriptName) == 0)
		{
			pVariable->ResetToDefault();
			return;
		}
	}

	char aBuf[IConsole::CMDLINE_LENGTH + 32];
	str_format(aBuf, sizeof(aBuf), "Invalid command: '%s'.", pScriptName);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
}

void CConfigManager::ResetGameSettings()
{
	for(SConfigVariable *pVariable : m_vpGameVariables)
	{
		pVariable->ResetToOld();
	}
}

void CConfigManager::SetReadOnly(const char *pScriptName, bool ReadOnly)
{
	for(SConfigVariable *pVariable : m_vpAllVariables)
	{
		if(str_comp(pScriptName, pVariable->m_pScriptName) == 0)
		{
			pVariable->m_ReadOnly = ReadOnly;
			return;
		}
	}
	char aBuf[IConsole::CMDLINE_LENGTH + 32];
	str_format(aBuf, sizeof(aBuf), "Invalid command for SetReadOnly: '%s'", pScriptName);
	dbg_assert(false, aBuf);
}

bool CConfigManager::Save()
{
	if(!m_pStorage || !g_Config.m_ClSaveSettings)
		return true;

	char aConfigFileTmp[IO_MAX_PATH_LENGTH];
	m_ConfigFile = m_pStorage->OpenFile(IStorage::FormatTmpPath(aConfigFileTmp, sizeof(aConfigFileTmp), CONFIG_FILE), IOFLAG_WRITE, IStorage::TYPE_SAVE);

	if(!m_ConfigFile)
	{
		log_error("config", "ERROR: opening %s failed", aConfigFileTmp);
		return false;
	}

	m_Failed = false;

	char aLineBuf[2048];
	for(const SConfigVariable *pVariable : m_vpAllVariables)
	{
		if((pVariable->m_Flags & CFGFLAG_SAVE) != 0 && !pVariable->IsDefault())
		{
			pVariable->Serialize(aLineBuf, sizeof(aLineBuf));
			WriteLine(aLineBuf);
		}
	}

	for(const auto &Callback : m_vCallbacks)
	{
		Callback.m_pfnFunc(this, Callback.m_pUserData);
	}

	for(const char *pCommand : m_vpUnknownCommands)
	{
		WriteLine(pCommand);
	}

	if(io_sync(m_ConfigFile) != 0)
	{
		m_Failed = true;
	}

	if(io_close(m_ConfigFile) != 0)
		m_Failed = true;

	m_ConfigFile = 0;

	if(m_Failed)
	{
		log_error("config", "ERROR: writing to %s failed", aConfigFileTmp);
		return false;
	}

	if(!m_pStorage->RenameFile(aConfigFileTmp, CONFIG_FILE, IStorage::TYPE_SAVE))
	{
		log_error("config", "ERROR: renaming %s to " CONFIG_FILE " failed", aConfigFileTmp);
		return false;
	}

	return true;
}

void CConfigManager::RegisterCallback(SAVECALLBACKFUNC pfnFunc, void *pUserData)
{
	m_vCallbacks.emplace_back(pfnFunc, pUserData);
}

void CConfigManager::WriteLine(const char *pLine)
{
	if(!m_ConfigFile ||
		io_write(m_ConfigFile, pLine, str_length(pLine)) != static_cast<unsigned>(str_length(pLine)) ||
		!io_write_newline(m_ConfigFile))
	{
		m_Failed = true;
	}
}

void CConfigManager::StoreUnknownCommand(const char *pCommand)
{
	m_vpUnknownCommands.push_back(m_ConfigHeap.StoreString(pCommand));
}

void CConfigManager::Con_Reset(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CConfigManager *>(pUserData)->Reset(pResult->GetString(0));
}

void CConfigManager::Con_Toggle(IConsole::IResult *pResult, void *pUserData)
{
	CConfigManager *pConfigManager = static_cast<CConfigManager *>(pUserData);
	IConsole *pConsole = pConfigManager->m_pConsole;

	const char *pScriptName = pResult->GetString(0);
	for(SConfigVariable *pVariable : pConfigManager->m_vpAllVariables)
	{
		if((pVariable->m_Flags & pConsole->FlagMask()) == 0 ||
			str_comp(pScriptName, pVariable->m_pScriptName) != 0)
		{
			continue;
		}

		if(pVariable->m_Type == SConfigVariable::VAR_INT)
		{
			SIntConfigVariable *pIntVariable = static_cast<SIntConfigVariable *>(pVariable);
			pIntVariable->SetValue(*pIntVariable->m_pVariable == pResult->GetInteger(1) ? pResult->GetInteger(2) : pResult->GetInteger(1));
		}
		else if(pVariable->m_Type == SConfigVariable::VAR_COLOR)
		{
			SColorConfigVariable *pColorVariable = static_cast<SColorConfigVariable *>(pVariable);
			const float Darkest = pColorVariable->m_Light ? 0.5f : 0.0f;
			const ColorHSLA Value = *pColorVariable->m_pVariable == pResult->GetColor(1, pColorVariable->m_Light).Pack(Darkest, pColorVariable->m_Alpha) ? pResult->GetColor(2, pColorVariable->m_Light) : pResult->GetColor(1, pColorVariable->m_Light);
			pColorVariable->SetValue(Value.Pack(Darkest, pColorVariable->m_Alpha));
		}
		else if(pVariable->m_Type == SConfigVariable::VAR_STRING)
		{
			SStringConfigVariable *pStringVariable = static_cast<SStringConfigVariable *>(pVariable);
			pStringVariable->SetValue(str_comp(pStringVariable->m_pStr, pResult->GetString(1)) == 0 ? pResult->GetString(2) : pResult->GetString(1));
		}
		return;
	}

	char aBuf[IConsole::CMDLINE_LENGTH + 32];
	str_format(aBuf, sizeof(aBuf), "Invalid command: '%s'.", pScriptName);
	pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
}

void CConfigManager::Con_ToggleStroke(IConsole::IResult *pResult, void *pUserData)
{
	CConfigManager *pConfigManager = static_cast<CConfigManager *>(pUserData);
	IConsole *pConsole = pConfigManager->m_pConsole;

	const char *pScriptName = pResult->GetString(1);
	for(SConfigVariable *pVariable : pConfigManager->m_vpAllVariables)
	{
		if((pVariable->m_Flags & pConsole->FlagMask()) == 0 ||
			pVariable->m_Type != SConfigVariable::VAR_INT ||
			str_comp(pScriptName, pVariable->m_pScriptName) != 0)
		{
			continue;
		}

		SIntConfigVariable *pIntVariable = static_cast<SIntConfigVariable *>(pVariable);
		pIntVariable->SetValue(pResult->GetInteger(0) == 0 ? pResult->GetInteger(3) : pResult->GetInteger(2));
		return;
	}

	char aBuf[IConsole::CMDLINE_LENGTH + 32];
	str_format(aBuf, sizeof(aBuf), "Invalid command: '%s'.", pScriptName);
	pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
}

IConfigManager *CreateConfigManager() { return new CConfigManager; }
