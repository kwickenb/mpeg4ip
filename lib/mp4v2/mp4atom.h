/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is MPEG4IP.
 * 
 * The Initial Developer of the Original Code is Cisco Systems Inc.
 * Portions created by Cisco Systems Inc. are
 * Copyright (C) Cisco Systems Inc. 2001.  All Rights Reserved.
 * 
 * Contributor(s): 
 *		Dave Mackie		dmackie@cisco.com
 */

#ifndef __MP4_ATOM_INCLUDED__
#define __MP4_ATOM_INCLUDED__

class MP4Atom;
MP4ARRAY_DECL(MP4Atom, MP4Atom*);

#define Required	true
#define Optional	false
#define OnlyOne		true
#define Many		false
#define Counted		true

/* helper class */
class MP4AtomInfo {
public:
	MP4AtomInfo() {
		m_name = NULL;
	}
	MP4AtomInfo(char* name, bool mandatory, bool onlyOne) {
		m_name = name;
		m_mandatory = mandatory;
		m_onlyOne = onlyOne;
		m_count = 0;
	}

	char* m_name;
	bool m_mandatory;
	bool m_onlyOne;
	u_int32_t m_count;
};

MP4ARRAY_DECL(MP4AtomInfo, MP4AtomInfo*);

class MP4Atom {
public:
	MP4Atom(char* type = NULL) {
		SetType(type);
		m_unknownType = FALSE;
		m_pFile = NULL;
		m_start = 0;
		m_end = 0;
		m_size = 0;
		m_pParentAtom = NULL;
	}
	virtual ~MP4Atom() { };

	static MP4Atom* ReadAtom(MP4File* pFile, MP4Atom* pParentAtom);
	static MP4Atom* CreateAtom(char* type);
	static bool IsReasonableType(const char* type);

	MP4File* GetFile() {
		return m_pFile;
	};
	void SetFile(MP4File* pFile) {
		m_pFile = pFile;
	};

	u_int64_t GetStart() {
		return m_start;
	};
	void SetStart(u_int64_t pos) {
		m_start = pos;
	};

	u_int64_t GetEnd() {
		return m_end;
	};
	void SetEnd(u_int64_t pos) {
		m_end = pos;
	};

	u_int64_t GetSize() {
		return m_size;
	}
	void SetSize(u_int64_t size) {
		m_size = size;
	}

	const char* GetType() {
		return m_type;
	};
	void SetType(char* type) {
		if (type) {
			WARNING(strlen(type) != 4);
			memcpy(m_type, type, 4);
			m_type[4] = '\0';
		} else {
			memset(m_type, 0, 5);
		}
	}

	void GetExtendedType(u_int8_t* pExtendedType) {
		memcpy(pExtendedType, m_extendedType, sizeof(m_extendedType));
	};
	void SetExtendedType(u_int8_t* pExtendedType) {
		memcpy(m_extendedType, pExtendedType, sizeof(m_extendedType));
	};

	bool IsUnknownType() {
		return m_unknownType;
	}
	void SetUnknownType(bool unknownType = true) {
		m_unknownType = unknownType;
	}

	bool IsRootAtom() {
		return m_type[0] == '\0';
	}

	MP4Atom* GetParentAtom() {
		return m_pParentAtom;
	}
	void SetParentAtom(MP4Atom* pParentAtom) {
		m_pParentAtom = pParentAtom;
	}

	void AddChildAtom(MP4Atom* pChildAtom) {
		pChildAtom->SetFile(m_pFile);
		pChildAtom->SetParentAtom(this);
		m_pChildAtoms.Add(pChildAtom);
	}

	void DeleteChildAtom(MP4Atom* pChildAtom) {
		for (MP4ArrayIndex i = 0; i < m_pChildAtoms.Size(); i++) {
			if (m_pChildAtoms[i] == pChildAtom) {
				m_pChildAtoms.Delete(i);
				return;
			}
		}
	}

	MP4Atom* FindAtom(char* name);

	bool FindProperty(char* name, 
		MP4Property** ppProperty, u_int32_t* pIndex = NULL);

	u_int8_t GetDepth();

	void Skip();

	virtual void Generate();
	virtual void Read();
	virtual void BeginWrite(bool use64 = false);
	virtual void Write();
	virtual void FinishWrite(bool use64 = false);
	virtual void Dump(FILE* pFile);

protected:
	void AddProperty(MP4Property* pProperty) {
		ASSERT(pProperty);
		m_pProperties.Add(pProperty);
		pProperty->SetParentAtom(this);
	}

	void AddVersionAndFlags() {
		AddProperty(new MP4Integer8Property("version"));
		AddProperty(new MP4Integer24Property("flags"));
	}

	void AddReserved(char* name, u_int32_t size) {
		MP4BytesProperty* pReserved = new MP4BytesProperty(name, size); 
		pReserved->SetReadOnly();
		AddProperty(pReserved);
	}

	void ExpectChildAtom(char* name, bool mandatory, bool onlyOne = true) {
		m_pChildAtomInfos.Add(new MP4AtomInfo(name, mandatory, onlyOne));
	}

	MP4AtomInfo* FindAtomInfo(const char* name);

	bool IsMe(char* name);

	MP4Atom* FindChildAtom(char* name);

	bool FindContainedProperty(char* name, 
		MP4Property** ppProperty, u_int32_t* pIndex);

	void ReadProperties(
		u_int32_t startIndex = 0, u_int32_t count = 0xFFFFFFFF);
	void ReadChildAtoms();

	void WriteProperties(
		u_int32_t startIndex = 0, u_int32_t count = 0xFFFFFFFF);
	void WriteChildAtoms();

	u_int8_t GetVersion();
	void SetVersion(u_int8_t version);

	u_int32_t GetFlags();
	void SetFlags(u_int32_t flags);

	/* debugging aid */
	u_int32_t GetVerbosity();

protected:
	MP4File*	m_pFile;
	u_int64_t	m_start;
	u_int64_t	m_end;
	u_int64_t	m_size;
	char		m_type[5];
	bool		m_unknownType;
	u_int8_t	m_extendedType[16];

	MP4Atom*	m_pParentAtom;

	MP4PropertyArray	m_pProperties;
	MP4AtomInfoArray 	m_pChildAtomInfos;
	MP4AtomArray		m_pChildAtoms;
};

inline u_int32_t ATOMID(const char* type) {
	return STRTOINT32(type);
}

#endif /* __MP4_ATOM_INCLUDED__ */
