//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "main.h"

#include <wx/wfstream.h>
#include <wx/tarstrm.h>
#include <wx/zstream.h>
#include <wx/mstream.h>
#include <wx/datstrm.h>
#include <wx/dir.h>

#include "settings.h"
#include "gui.h" // Loadbar

#include "creatures.h"
#include "creature.h"
#include "map.h"
#include "tile.h"
#include "item.h"
#include "complexitem.h"
#include "town.h"

#include "iomap_otbm.h"

typedef uint8_t attribute_t;
typedef uint32_t flags_t;


std::map<uint16_t, std::vector<Position>> zoneMap;

// H4X
void reform(Map* map, Tile* tile, Item* item)
{
	/*
	int aid = item->getActionID();
	int id = item->getID();
	int uid = item->getUniqueID();

	if(item->isDoor()) {
		item->eraseAttribute("aid");
		item->setAttribute("keyid", aid);
	}

	if((item->isDoor()) && tile && tile->getHouseID()) {
		Door* self = static_cast<Door*>(item);
		House* house = map->houses.getHouse(tile->getHouseID());
		self->setDoorID(house->getEmptyDoorID());
	}
	*/
}

// ============================================================================
// Item

std::string removeOTBMExtension(const std::string& filename) {
	size_t pos = filename.rfind(".otbm");
	if (pos != std::string::npos) {
		return filename.substr(0, pos);
	}
	return filename;
}

toml::table serializeZoneToToml(uint16_t zoneId, const std::vector<Position>& positions) {
	std::ostringstream tomlStream;
	tomlStream << "[[zone]]\n";
	tomlStream << "id = " << zoneId << "\n";
	tomlStream << "positions = [";
	for (size_t i = 0; i < positions.size(); ++i) {
		const auto& pos = positions[i];
		tomlStream << "{x = " << pos.x << ", y = " << pos.y << ", z = " << pos.z << "}";
		if (i < positions.size() - 1) {
			tomlStream << ", ";
		}
	}
	tomlStream << "]\n\n";
	std::string tomlString = tomlStream.str();
	try {
		return toml::parse(tomlString);
	}
	catch (const toml::parse_error& err) {
		throw std::runtime_error("Failed to parse generated TOML: " + std::string(err.what()));
	}
}

void IOMapOTBM::saveZonesToToml(const std::map<uint16_t, std::vector<Position>>& zoneMap, const wxFileName& dir, Map& map) {
	auto mapName = removeOTBMExtension(map.getName());
	auto folderPath = dir.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME) + mapName + "-zones";

	if (not wxDirExists(folderPath)) {
		wxMkDir(folderPath);
	}

	wxArrayString existingFiles;
	wxDir::GetAllFiles(folderPath, &existingFiles, "*.toml", wxDIR_FILES);

	std::set<uint16_t> zoneIdsToSave;
	for (const auto& [zoneId, _] : zoneMap) {
		zoneIdsToSave.insert(zoneId);
	}

	for (const auto& filePath : existingFiles) {
		auto fileName = wxFileName(filePath);
		auto baseName = fileName.GetName();

		unsigned long zoneId;
		if (baseName.ToULong(&zoneId)) {
			if (zoneIdsToSave.find(static_cast<uint16_t>(zoneId)) == zoneIdsToSave.end()) {
				if (!wxRemoveFile(filePath)) {
					wxLogError("Failed to delete zone file: %s", filePath);
				}
			}
		}
	}

	for (const auto& [zoneId, positions] : zoneMap) {
		auto zoneTable = serializeZoneToToml(zoneId, positions);
		auto filepath = folderPath + wxString::Format("/%u.toml", zoneId);
		std::stringstream ss;
		ss << zoneTable;
		auto file = wxFile(filepath, wxFile::write);
		if (file.IsOpened()) {
			auto zoneData = ss.str();
			file.Write(zoneData.c_str(), zoneData.length());
			file.Close();
		}
		else {
			wxLogError("Failed to open file for writing: %s", filepath);
		}
	}
}

std::map<uint16_t, std::vector<Position>> loadZonesFromToml(wxString folderPath) {
	std::map<uint16_t, std::vector<Position>> zoneMap;

	if (not wxDirExists(folderPath)) {
		return zoneMap;
	}
	auto zoneDir = wxDir(folderPath);
	if (not zoneDir.IsOpened()) {
		return zoneMap;
	}
	wxString filename;
	bool open_file = zoneDir.GetFirst(&filename, "*.toml", wxDIR_FILES);
	while (open_file) {
		wxString filepath = folderPath + "/" + filename;
		auto file = wxFile(filepath, wxFile::read);
		if (file.IsOpened()) {
			auto fileLength = file.Length();
			if (fileLength > 0) {
				std::string fileContent;
				fileContent.resize(static_cast<size_t>(fileLength));
				file.Read(&fileContent[0], fileLength);
				try {
					toml::table tbl = toml::parse(fileContent);
					auto zonesArray = tbl["zone"];
					if (zonesArray && zonesArray.is_array()) {
						for (const auto& zoneEntry : *zonesArray.as_array()) {
							if (zoneEntry.is_table()) {
								const toml::table& zoneTable = *zoneEntry.as_table();
								auto idVal = zoneTable["id"];
								auto posArray = zoneTable["positions"];
								if (idVal && idVal.is_integer() && posArray && posArray.is_array()) {
									uint16_t zoneId = static_cast<uint16_t>(idVal.value_or(0));
									std::vector<Position> positions;
									for (const auto& pos : *posArray.as_array()) {
										if (pos.is_table()) {
											const toml::table& posTable = *pos.as_table();
											uint16_t x = static_cast<uint16_t>(posTable["x"].value_or(0));
											uint16_t y = static_cast<uint16_t>(posTable["y"].value_or(0));
											uint8_t z = static_cast<uint8_t>(posTable["z"].value_or(0));
											positions.emplace_back(Position{ x, y, z });
										}
									}
									zoneMap[zoneId] = std::move(positions);
								}
							}
						}
					}
					else {
						wxLogWarning("Invalid zone file: %s", filepath);
					}
				}
				catch (const toml::parse_error& err) {
					wxLogError("TOML parse error in file %s: %s", filepath, err.what());
				}
			}
			file.Close();
		}
		open_file = zoneDir.GetNext(&filename);
	}
	return zoneMap;
}

void applyZonesToTiles(const std::map<uint16_t, std::vector<Position>>& zoneMap, BaseMap& map) {
	if (not zoneMap.empty())
	{
		for (const auto& [zoneId, positions] : zoneMap)
		{
			for (const auto& pos : positions)
			{
				Tile* tile = map.getTile(pos);
				if (tile)
				{
					tile->setMapFlags(tile->getMapFlags() | TILESTATE_ZONE_BRUSH);
					tile->addZoneId(zoneId);
				}
			}
		}
	}
}

Item* Item::Create_OTBM(const IOMap& maphandle, BinaryNode* stream)
{
	uint16_t id;
	if(!stream->getU16(id)) {
		return nullptr;
	}

	const ItemType& type = g_items.getItemType(id);
	uint8_t count = 0;
	if(maphandle.version.otbm == MAP_OTBM_1) {
		if(type.stackable || type.isSplash() || type.isFluidContainer()) {
			stream->getU8(count);
		}
	}
	return Item::Create(id, count);
}

bool Item::readItemAttribute_OTBM(const IOMap& maphandle, OTBM_ItemAttribute attr, BinaryNode* stream)
{
	switch (attr) {
		case OTBM_ATTR_COUNT: {
			uint8_t subtype;
			if(!stream->getU8(subtype)) {
				return false;
			}
			setSubtype(subtype);
			break;
		}
		case OTBM_ATTR_ACTION_ID: {
			uint16_t aid;
			if(!stream->getU16(aid)) {
				return false;
			}
			setActionID(aid);
			break;
		}
		case OTBM_ATTR_UNIQUE_ID: {
			uint16_t uid;
			if(!stream->getU16(uid)) {
				return false;
			}
			setUniqueID(uid);
			break;
		}
		case OTBM_ATTR_CHARGES: {
			uint16_t charges;
			if(!stream->getU16(charges)) {
				return false;
			}
			setSubtype(charges);
			break;
		}
		case OTBM_ATTR_TEXT: {
			std::string text;
			if(!stream->getString(text)) {
				return false;
			}
			setText(text);
			break;
		}
		case OTBM_ATTR_DESC: {
			std::string text;
			if(!stream->getString(text)) {
				return false;
			}
			setDescription(text);
			break;
		}
		case OTBM_ATTR_RUNE_CHARGES: {
			uint8_t subtype;
			if(!stream->getU8(subtype)) {
				return false;
			}
			setSubtype(subtype);
			break;
		}

		// The following *should* be handled in the derived classes
		// However, we still need to handle them here since otherwise things
		// will break horribly
		case OTBM_ATTR_DEPOT_ID: return stream->skip(2);
		case OTBM_ATTR_HOUSEDOORID: return stream->skip(1);
		case OTBM_ATTR_TELE_DEST: return stream->skip(5);
		default: return false;
	}
	return true;
}

bool Item::unserializeAttributes_OTBM(const IOMap& maphandle, BinaryNode* stream)
{
	uint8_t attribute;
	while(stream->getU8(attribute)) {
		if(attribute == OTBM_ATTR_ATTRIBUTE_MAP) {
			if(!ItemAttributes::unserializeAttributeMap(maphandle, stream)) {
				return false;
			}
		} else if(!readItemAttribute_OTBM(maphandle, static_cast<OTBM_ItemAttribute>(attribute), stream)) {
			return false;
		}
	}
	return true;
}

bool Item::unserializeItemNode_OTBM(const IOMap& maphandle, BinaryNode* node)
{
	return unserializeAttributes_OTBM(maphandle, node);
}

void Item::serializeItemAttributes_OTBM(const IOMap& maphandle, NodeFileWriteHandle& stream) const
{
	if(maphandle.version.otbm >= MAP_OTBM_2) {
		const ItemType& type = g_items.getItemType(id);
		if(type.stackable || type.isSplash() || type.isFluidContainer()) {
			stream.addU8(OTBM_ATTR_COUNT);
			stream.addU8(getSubtype());
		}
	}

	if(maphandle.version.otbm >= MAP_OTBM_4) {
		if(attributes && !attributes->empty()) {
			stream.addU8(OTBM_ATTR_ATTRIBUTE_MAP);
			serializeAttributeMap(maphandle, stream);
		}
	} else {
		if(g_items.MinorVersion >= CLIENT_VERSION_820 && isCharged()) {
			stream.addU8(OTBM_ATTR_CHARGES);
			stream.addU16(getSubtype());
		}

		uint16_t actionId = getActionID();
		if(actionId > 0) {
			stream.addU8(OTBM_ATTR_ACTION_ID);
			stream.addU16(actionId);
		}

		uint16_t uniqueId = getUniqueID();
		if(uniqueId > 0) {
			stream.addU8(OTBM_ATTR_UNIQUE_ID);
			stream.addU16(uniqueId);
		}

		const std::string& text = getText();
		if(!text.empty()) {
			stream.addU8(OTBM_ATTR_TEXT);
			stream.addString(text);
		}

		const std::string& description = getDescription();
		if(!description.empty()) {
			stream.addU8(OTBM_ATTR_DESC);
			stream.addString(description);
		}
	}
}

void Item::serializeItemCompact_OTBM(const IOMap& maphandle, NodeFileWriteHandle& stream) const
{
	stream.addU16(id);

	/* This is impossible
	const ItemType& iType = g_items[id];

	if(iType.stackable || iType.isSplash() || iType.isFluidContainer()){
		stream.addU8(getSubtype());
	}
	*/
}

bool Item::serializeItemNode_OTBM(const IOMap& maphandle, NodeFileWriteHandle& file) const
{
	file.addNode(OTBM_ITEM);
	file.addU16(id);
	if(maphandle.version.otbm == MAP_OTBM_1) {
		const ItemType& type = g_items.getItemType(id);
		if(type.stackable || type.isSplash() || type.isFluidContainer()) {
			file.addU8(getSubtype());
		}
	}
	serializeItemAttributes_OTBM(maphandle, file);
	file.endNode();
	return true;
}

// ============================================================================
// Teleport

bool Teleport::readItemAttribute_OTBM(const IOMap& maphandle, OTBM_ItemAttribute attribute, BinaryNode* stream)
{
	if(OTBM_ATTR_TELE_DEST == attribute) {
		uint16_t x, y;
		uint8_t z;
		if(!stream->getU16(x) || !stream->getU16(y) || !stream->getU8(z)) {
			return false;
		}
		destination = Position(x, y, z);
		return true;
	} else {
		return Item::readItemAttribute_OTBM(maphandle, attribute, stream);
	}
}

void Teleport::serializeItemAttributes_OTBM(const IOMap& maphandle, NodeFileWriteHandle& stream) const
{
	Item::serializeItemAttributes_OTBM(maphandle, stream);

	stream.addByte(OTBM_ATTR_TELE_DEST);
	stream.addU16(destination.x);
	stream.addU16(destination.y);
	stream.addU8(destination.z);
}

// ============================================================================
// Door

bool Door::readItemAttribute_OTBM(const IOMap& maphandle, OTBM_ItemAttribute attribute, BinaryNode* stream)
{
	if(OTBM_ATTR_HOUSEDOORID == attribute) {
		uint8_t id = 0;
		if(!stream->getU8(id)) {
			return false;
		}
		doorId = id;
		return true;
	} else {
		return Item::readItemAttribute_OTBM(maphandle, attribute, stream);
	}
}

void Door::serializeItemAttributes_OTBM(const IOMap& maphandle, NodeFileWriteHandle& stream) const
{
	Item::serializeItemAttributes_OTBM(maphandle, stream);
	if(doorId) {
		stream.addByte(OTBM_ATTR_HOUSEDOORID);
		stream.addU8(doorId);
	}
}

// ============================================================================
// Depots

bool Depot::readItemAttribute_OTBM(const IOMap& maphandle, OTBM_ItemAttribute attribute, BinaryNode* stream)
{
	if(OTBM_ATTR_DEPOT_ID == attribute) {
		uint16_t id = 0;
		if(!stream->getU16(id)) {
			return false;
		}
		depotId = id;
		return true;
	} else {
		return Item::readItemAttribute_OTBM(maphandle, attribute, stream);
	}
}

void Depot::serializeItemAttributes_OTBM(const IOMap& maphandle, NodeFileWriteHandle& stream) const
{
	Item::serializeItemAttributes_OTBM(maphandle, stream);
	if(depotId) {
		stream.addByte(OTBM_ATTR_DEPOT_ID);
		stream.addU16(depotId);
	}
}

// ============================================================================
// Container

bool Container::unserializeItemNode_OTBM(const IOMap& maphandle, BinaryNode* node)
{
	if(!Item::unserializeAttributes_OTBM(maphandle, node)) {
		return false;
	}

	BinaryNode* child = node->getChild();
	if(child) {
		do {
			uint8_t type;
			if(!child->getByte(type)) {
				return false;
			}

			if(type != OTBM_ITEM) {
				return false;
			}

			Item* item = Item::Create_OTBM(maphandle, child);
			if(!item) {
				return false;
			}

			if(!item->unserializeItemNode_OTBM(maphandle, child)) {
				delete item;
				return false;
			}

			contents.push_back(item);
		} while(child->advance());
	}
	return true;
}

bool Container::serializeItemNode_OTBM(const IOMap& maphandle, NodeFileWriteHandle& file) const
{
	file.addNode(OTBM_ITEM);
	file.addU16(id);
	if(maphandle.version.otbm == MAP_OTBM_1) {
		// In the ludicrous event that an item is a container AND stackable, we have to do this. :p
		const ItemType& type = g_items.getItemType(id);
		if(type.stackable || type.isSplash() || type.isFluidContainer()) {
			file.addU8(getSubtype());
		}
	}

	serializeItemAttributes_OTBM(maphandle, file);
	for(Item* item : contents) {
		item->serializeItemNode_OTBM(maphandle, file);
	}

	file.endNode();
	return true;
}

/*
	OTBM_ROOTV1
	|
	|--- OTBM_MAP_DATA
	|	|
	|	|--- OTBM_TILE_AREA
	|	|	|--- OTBM_TILE
	|	|	|--- OTBM_TILE_SQUARE (not implemented)
	|	|	|--- OTBM_TILE_REF (not implemented)
	|	|	|--- OTBM_HOUSETILE
	|	|
	|	|--- OTBM_SPAWNS (not implemented)
	|	|	|--- OTBM_SPAWN_AREA (not implemented)
	|	|	|--- OTBM_MONSTER (not implemented)
	|	|
	|	|--- OTBM_TOWNS
	|		|--- OTBM_TOWN
	|
	|--- OTBM_ITEM_DEF (not implemented)
*/

bool IOMapOTBM::getVersionInfo(const FileName& filename, MapVersion& out_ver)
{
	// Just open a disk-based read handle
	DiskNodeFileReadHandle f(nstr(filename.GetFullPath()), StringVector(1, "OTBM"));
	if(!f.isOk())
		return false;
	return getVersionInfo(&f, out_ver);
}

bool IOMapOTBM::getVersionInfo(NodeFileReadHandle* f,  MapVersion& out_ver)
{
	BinaryNode* root = f->getRootNode();
	if(!root)
		return false;

	root->skip(1); // Skip the type byte

	uint16_t u16;
	uint32_t u32;

	if(!root->getU32(u32)) // Version
		return false;
	out_ver.otbm = (MapVersionID)u32;


	root->getU16(u16);
	root->getU16(u16);
	root->getU32(u32);

	if(!root->getU32(u32)) // OTB minor version
		return false;

	out_ver.client = ClientVersionID(u32);
	return true;
}

bool IOMapOTBM::loadMap(Map& map, const FileName& filename)
{
	DiskNodeFileReadHandle f(nstr(filename.GetFullPath()), StringVector(1, "OTBM"));
	if(!f.isOk()) {
		error(("Couldn't open file for reading\nThe error reported was: " + wxstr(f.getErrorMessage())).wc_str());
		return false;
	}

	if(!loadMap(map, f))
		return false;

	// Read auxilliary files
	if(!loadHouses(map, filename)) {
		warning("Failed to load houses.");
		map.housefile = nstr(filename.GetName()) + "-house.xml";
	}
	if(!loadSpawns(map, filename)) {
		warning("Failed to load spawns.");
		map.spawnfile = nstr(filename.GetName()) + "-spawn.xml";
	}

	auto mapName = nstr(filename.GetName());

	auto zoneDir = filename.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME) + mapName + "-zones";

	auto zoneMap = loadZonesFromToml(zoneDir);
	applyZonesToTiles(zoneMap, map);

	return true;
}

bool IOMapOTBM::loadMap(Map& map, NodeFileReadHandle& f)
{
	BinaryNode* root = f.getRootNode();
	if(!root) {
		error("Could not read root node.");
		return false;
	}
	root->skip(1); // Skip the type byte

	uint8_t u8;
	uint16_t u16;
	uint32_t u32;

	if(!root->getU32(u32))
		return false;

	version.otbm = (MapVersionID) u32;

	if(version.otbm > MAP_OTBM_4) {
		// Failed to read version
		if(g_gui.PopupDialog("Map error",
			"The loaded map appears to be a OTBM format that is not supported by the editor."
			"Do you still want to attempt to load the map?", wxYES | wxNO) == wxID_YES)
		{
			warning("Unsupported or damaged map version");
		} else {
			error("Unsupported OTBM version, could not load map");
			return false;
		}
	}

	if(!root->getU16(u16))
		return false;

	map.width = u16;
	if(!root->getU16(u16))
		return false;

	map.height = u16;

	if(!root->getU32(u32) || u32 > (unsigned long)g_items.MajorVersion) { // OTB major version
		if(g_gui.PopupDialog("Map error",
			"The loaded map appears to be a items.otb format that deviates from the "
			"items.otb loaded by the editor. Do you still want to attempt to load the map?", wxYES | wxNO) == wxID_YES)
		{
			warning("Map was saved with different .otb than the version of current one. Caution - Might be unsupported or damaged map version");
		} else {
			error("Outdated items.otb, could not load map");
			return false;
		}
	}

	if(!root->getU32(u32) || u32 > (unsigned long)g_items.MinorVersion) { // OTB minor version
		warning("This editor needs an updated items.otb version");
	}
	version.client = (ClientVersionID)u32;

	BinaryNode* mapHeaderNode = root->getChild();
	if(mapHeaderNode == nullptr || !mapHeaderNode->getByte(u8) || u8 != OTBM_MAP_DATA) {
		error("Could not get root child node. Cannot recover from fatal error!");
		return false;
	}

	uint8_t attribute;
	while(mapHeaderNode->getU8(attribute)) {
		switch(attribute) {
			case OTBM_ATTR_DESCRIPTION: {
				if(!mapHeaderNode->getString(map.description)) {
					warning("Invalid map description tag");
				}
				//std::cout << "Map description: " << mapDescription << std::endl;
				break;
			}
			case OTBM_ATTR_EXT_SPAWN_FILE: {
				if(!mapHeaderNode->getString(map.spawnfile)) {
					warning("Invalid map spawnfile tag");
				}
				break;
			}
			case OTBM_ATTR_EXT_HOUSE_FILE: {
				if(!mapHeaderNode->getString(map.housefile)) {
					warning("Invalid map housefile tag");
				}
				break;
			}
			default: {
				warning("Unknown header node.");
				break;
			}
		}
	}

	int nodes_loaded = 0;

	for(BinaryNode* mapNode = mapHeaderNode->getChild(); mapNode != nullptr; mapNode = mapNode->advance()) {
		++nodes_loaded;
		if(nodes_loaded % 15 == 0) {
			g_gui.SetLoadDone(static_cast<int32_t>(100.0 * f.tell() / f.size()));
		}

		uint8_t node_type;
		if(!mapNode->getByte(node_type)) {
			warning("Invalid map node");
			continue;
		}
		if(node_type == OTBM_TILE_AREA) {
			uint16_t base_x, base_y;
			uint8_t base_z;
			if(!mapNode->getU16(base_x) || !mapNode->getU16(base_y) || !mapNode->getU8(base_z)) {
				warning("Invalid map node, no base coordinate");
				continue;
			}

			for(BinaryNode* tileNode = mapNode->getChild(); tileNode != nullptr; tileNode = tileNode->advance()) {
				Tile* tile = nullptr;
				uint8_t tile_type;
				if(!tileNode->getByte(tile_type)) {
					warning("Invalid tile type");
					continue;
				}
				if(tile_type == OTBM_TILE || tile_type == OTBM_HOUSETILE) {
					//printf("Start\n");
					uint8_t x_offset, y_offset;
					if(!tileNode->getU8(x_offset) || !tileNode->getU8(y_offset)) {
						warning("Could not read position of tile");
						continue;
					}
					const Position pos(base_x + x_offset, base_y + y_offset, base_z);

					if(map.getTile(pos)) {
						warning("Duplicate tile at %d:%d:%d, discarding duplicate", pos.x, pos.y, pos.z);
						continue;
					}

					tile = map.allocator(map.createTileL(pos));
					House* house = nullptr;
					if(tile_type == OTBM_HOUSETILE) {
						uint32_t house_id;
						if(!tileNode->getU32(house_id)) {
							warning("House tile without house data, discarding tile");
							continue;
						}
						if(house_id) {
							house = map.houses.getHouse(house_id);
							if(!house) {
								house = newd House(map);
								house->id = house_id;
								map.houses.addHouse(house);
							}
						} else {
							warning("Invalid house id from tile %d:%d:%d", pos.x, pos.y, pos.z);
						}
					}

					uint8_t attribute;
					while(tileNode->getU8(attribute)) {
						switch(attribute) {
							case OTBM_ATTR_TILE_FLAGS: {
								uint32_t flags = 0;
								if(!tileNode->getU32(flags)) {
									warning("Invalid tile flags of tile on %d:%d:%d", pos.x, pos.y, pos.z);
								}

								tile->setMapFlags(flags);
								break;
							}
							case OTBM_ATTR_ITEM: {
								Item* item = Item::Create_OTBM(*this, tileNode);
								if(item == nullptr)
								{
									warning("Invalid item at tile %d:%d:%d", pos.x, pos.y, pos.z);
								}
								tile->addItem(item);
								break;
							}
							default: {
								warning("Unknown tile attribute at %d:%d:%d", pos.x, pos.y, pos.z);
								break;
							}
						}
					}

					for(BinaryNode* itemNode = tileNode->getChild(); itemNode != nullptr; itemNode = itemNode->advance()) {
						Item* item = nullptr;
						uint8_t item_type;
						if(!itemNode->getByte(item_type)) {
							warning("Unknown item type %d:%d:%d", pos.x, pos.y, pos.z);
							continue;
						}
						if(item_type == OTBM_ITEM) {
							item = Item::Create_OTBM(*this, itemNode);
							if(item) {
								if(!item->unserializeItemNode_OTBM(*this, itemNode)) {
									warning("Couldn't unserialize item attributes at %d:%d:%d", pos.x, pos.y, pos.z);
								}
								//reform(&map, tile, item);
								tile->addItem(item);
							}
						} else {
							warning("Unknown type of tile child node");
						}
					}

					tile->update();
					if(house)
						house->addTile(tile);

					map.setTile(pos.x, pos.y, pos.z, tile);
				} else {
					warning("Unknown type of tile node");
				}
			}
		} else if(node_type == OTBM_TOWNS) {
			for(BinaryNode* townNode = mapNode->getChild(); townNode != nullptr; townNode = townNode->advance()) {
				Town* town = nullptr;
				uint8_t town_type;
				if(!townNode->getByte(town_type)) {
					warning("Invalid town type (1)");
					continue;
				}
				if(town_type != OTBM_TOWN) {
					warning("Invalid town type (2)");
					continue;
				}
				uint32_t town_id;
				if(!townNode->getU32(town_id)) {
					warning("Invalid town id");
					continue;
				}

				town = map.towns.getTown(town_id);
				if(town) {
					warning("Duplicate town id %d, discarding duplicate", town_id);
					continue;
				} else {
					town = newd Town(town_id);
					if(!map.towns.addTown(town)) {
						delete town;
						continue;
					}
				}
				std::string town_name;
				if(!townNode->getString(town_name)) {
					warning("Invalid town name");
					continue;
				}
				town->setName(town_name);
				Position pos;
				uint16_t x;
				uint16_t y;
				uint8_t z;
				if(!townNode->getU16(x) || !townNode->getU16(y) || !townNode->getU8(z)) {
					warning("Invalid town temple position");
					continue;
				}
				pos.x = x;
				pos.y = y;
				pos.z = z;
				town->setTemplePosition(pos);
			}
		} else if(node_type == OTBM_WAYPOINTS) {
			for(BinaryNode* waypointNode = mapNode->getChild(); waypointNode != nullptr; waypointNode = waypointNode->advance()) {
				uint8_t waypoint_type;
				if(!waypointNode->getByte(waypoint_type)) {
					warning("Invalid waypoint type (1)");
					continue;
				}
				if(waypoint_type != OTBM_WAYPOINT) {
					warning("Invalid waypoint type (2)");
					continue;
				}

				Waypoint wp;

				if(!waypointNode->getString(wp.name)) {
					warning("Invalid waypoint name");
					continue;
				}
				uint16_t x;
				uint16_t y;
				uint8_t z;
				if(!waypointNode->getU16(x) || !waypointNode->getU16(y) || !waypointNode->getU8(z)) {
					warning("Invalid waypoint position");
					continue;
				}
				wp.pos.x = x;
				wp.pos.y = y;
				wp.pos.z = z;

				map.waypoints.addWaypoint(newd Waypoint(wp));
			}
		}
	}

	if(!f.isOk())
		warning(wxstr(f.getErrorMessage()).wc_str());
	return true;
}

bool IOMapOTBM::loadSpawns(Map& map, const FileName& dir)
{
	std::string fn = (const char*)(dir.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME).mb_str(wxConvUTF8));
	fn += map.spawnfile;

	FileName filename(wxstr(fn));
	if(!filename.FileExists())
		return false;

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(fn.c_str());
	if(!result) {
		return false;
	}
	return loadSpawns(map, doc);
}

bool IOMapOTBM::loadSpawns(Map& map, pugi::xml_document& doc)
{
	pugi::xml_node node = doc.child("spawns");
	if(!node) {
		warnings.push_back("IOMapOTBM::loadSpawns: Invalid rootheader.");
		return false;
	}

	for(pugi::xml_node spawnNode = node.first_child(); spawnNode; spawnNode = spawnNode.next_sibling()) {
		if(as_lower_str(spawnNode.name()) != "spawn") {
			continue;
		}

		Position spawnPosition;
		spawnPosition.x = spawnNode.attribute("centerx").as_int();
		spawnPosition.y = spawnNode.attribute("centery").as_int();
		spawnPosition.z = spawnNode.attribute("centerz").as_int();

		if(spawnPosition.x == 0 || spawnPosition.y == 0) {
			warning("Bad position data on one spawn, discarding...");
			continue;
		}

		int32_t radius = spawnNode.attribute("radius").as_int();
		if(radius < 1) {
			warning("Couldn't read radius of spawn.. discarding spawn...");
			continue;
		}

		Tile* tile = map.getTile(spawnPosition);
		if(tile && tile->spawn) {
			warning("Duplicate spawn on position %d:%d:%d\n", tile->getX(), tile->getY(), tile->getZ());
			continue;
		}

		Spawn* spawn = newd Spawn(radius);
		if(!tile) {
			tile = map.allocator(map.createTileL(spawnPosition));
			map.setTile(spawnPosition, tile);
		}

		tile->spawn = spawn;
		map.addSpawn(tile);

		for(pugi::xml_node creatureNode = spawnNode.first_child(); creatureNode; creatureNode = creatureNode.next_sibling()) {
			const std::string& creatureNodeName = as_lower_str(creatureNode.name());
			if(creatureNodeName != "monster" && creatureNodeName != "npc") {
				continue;
			}

			bool isNpc = creatureNodeName == "npc";
			const std::string& name = creatureNode.attribute("name").as_string();
			if(name.empty()) {
				wxString err;
				err << "Bad creature position data, discarding creature at spawn " << spawnPosition.x << ":" << spawnPosition.y << ":" << spawnPosition.z << " due missing name.";
				warnings.Add(err);
				break;
			}

			int32_t spawntime = creatureNode.attribute("spawntime").as_int();
			if(spawntime == 0) {
				spawntime = g_settings.getInteger(Config::DEFAULT_SPAWNTIME);
			}

			Direction direction = NORTH;
			int dir = creatureNode.attribute("direction").as_int(-1);
			if(dir >= DIRECTION_FIRST && dir <= DIRECTION_LAST) {
				direction = (Direction)dir;
			}

			Position creaturePosition(spawnPosition);

			pugi::xml_attribute xAttribute = creatureNode.attribute("x");
			pugi::xml_attribute yAttribute = creatureNode.attribute("y");
			if(!xAttribute || !yAttribute) {
				wxString err;
				err << "Bad creature position data, discarding creature \"" << name << "\" at spawn " << creaturePosition.x << ":" << creaturePosition.y << ":" << creaturePosition.z << " due to invalid position.";
				warnings.Add(err);
				break;
			}

			creaturePosition.x += xAttribute.as_int();
			creaturePosition.y += yAttribute.as_int();

			radius = std::max<int32_t>(radius, std::abs(creaturePosition.x - spawnPosition.x));
			radius = std::max<int32_t>(radius, std::abs(creaturePosition.y - spawnPosition.y));
			radius = std::min<int32_t>(radius, g_settings.getInteger(Config::MAX_SPAWN_RADIUS));

			Tile* creatureTile;
			if(creaturePosition == spawnPosition) {
				creatureTile = tile;
			} else {
				creatureTile = map.getTile(creaturePosition);
			}

			if(!creatureTile) {
				wxString err;
				err << "Discarding creature \"" << name << "\" at " << creaturePosition.x << ":" << creaturePosition.y << ":" << creaturePosition.z << " due to invalid position.";
				warnings.Add(err);
				break;
			}

			if(creatureTile->creature) {
				wxString err;
				err << "Duplicate creature \"" << name << "\" at " << creaturePosition.x << ":" << creaturePosition.y << ":" << creaturePosition.z << " was discarded.";
				warnings.Add(err);
				break;
			}

			CreatureType* type = g_creatures[name];
			if(!type) {
				type = g_creatures.addMissingCreatureType(name, isNpc);
			}

			Creature* creature = newd Creature(type);
			creature->setDirection(direction);
			creature->setSpawnTime(spawntime);
			creatureTile->creature = creature;

			if(creatureTile->getLocation()->getSpawnCount() == 0) {
				// No spawn, create a newd one
				ASSERT(creatureTile->spawn == nullptr);
				Spawn* spawn = newd Spawn(5);
				creatureTile->spawn = spawn;
				map.addSpawn(creatureTile);
			}
		}
	}
	return true;
}

bool IOMapOTBM::loadHouses(Map& map, const FileName& dir)
{
	std::string fn = (const char*)(dir.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME).mb_str(wxConvUTF8));
	fn += map.housefile;
	FileName filename(wxstr(fn));
	if(!filename.FileExists())
		return false;

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(fn.c_str());
	if(!result) {
		return false;
	}
	return loadHouses(map, doc);
}

bool IOMapOTBM::loadHouses(Map& map, pugi::xml_document& doc)
{
	pugi::xml_node node = doc.child("houses");
	if(!node) {
		warnings.push_back("IOMapOTBM::loadHouses: Invalid rootheader.");
		return false;
	}

	pugi::xml_attribute attribute;
	for(pugi::xml_node houseNode = node.first_child(); houseNode; houseNode = houseNode.next_sibling()) {
		if(as_lower_str(houseNode.name()) != "house") {
			continue;
		}

		House* house = nullptr;
		if((attribute = houseNode.attribute("houseid"))) {
			house = map.houses.getHouse(attribute.as_uint());
			if(!house) {
				break;
			}
		}

		if((attribute = houseNode.attribute("name"))) {
			house->name = attribute.as_string();
		} else {
			house->name = "House #" + std::to_string(house->id);
		}

		Position exitPosition(
			houseNode.attribute("entryx").as_int(),
			houseNode.attribute("entryy").as_int(),
			houseNode.attribute("entryz").as_int()
		);
		if(exitPosition.x != 0 && exitPosition.y != 0 && exitPosition.z != 0) {
			house->setExit(exitPosition);
		}

		if((attribute = houseNode.attribute("rent"))) {
			house->rent = attribute.as_int();
		}

		if((attribute = houseNode.attribute("guildhall"))) {
			house->guildhall = attribute.as_bool();
		}

		if((attribute = houseNode.attribute("townid"))) {
			house->townid = attribute.as_uint();
		} else {
			warning("House %d has no town! House was removed.", house->id);
			map.houses.removeHouse(house);
		}
	}
	return true;
}

bool IOMapOTBM::saveMap(Map& map, const FileName& identifier)
{
#if OTGZ_SUPPORT > 0
	if(identifier.GetExt() == "otgz") {
		// Create the archive
		struct archive* a = archive_write_new();
		struct archive_entry* entry = nullptr;
		std::ostringstream streamData;

		archive_write_set_compression_gzip(a);
		archive_write_set_format_pax_restricted(a);
		archive_write_open_filename(a, nstr(identifier.GetFullPath()).c_str());

		g_gui.SetLoadDone(0, "Saving spawns...");

		pugi::xml_document spawnDoc;
		if(saveSpawns(map, spawnDoc)) {
			// Write the data
			spawnDoc.save(streamData, "", pugi::format_raw, pugi::encoding_utf8);
			std::string xmlData = streamData.str();

			// Write to the arhive
			entry = archive_entry_new();
			archive_entry_set_pathname(entry, "world/spawns.xml");
			archive_entry_set_size(entry, xmlData.size());
			archive_entry_set_filetype(entry, AE_IFREG);
			archive_entry_set_perm(entry, 0644);

			// Write to the archive
			archive_write_header(a, entry);
			archive_write_data(a, xmlData.data(), xmlData.size());

			// Free the entry
			archive_entry_free(entry);
			streamData.str("");
		}

		g_gui.SetLoadDone(0, "Saving houses...");

		pugi::xml_document houseDoc;
		if(saveHouses(map, houseDoc)) {
			// Write the data
			houseDoc.save(streamData, "", pugi::format_raw, pugi::encoding_utf8);
			std::string xmlData = streamData.str();

			// Write to the arhive
			entry = archive_entry_new();
			archive_entry_set_pathname(entry, "world/houses.xml");
			archive_entry_set_size(entry, xmlData.size());
			archive_entry_set_filetype(entry, AE_IFREG);
			archive_entry_set_perm(entry, 0644);

			// Write to the archive
			archive_write_header(a, entry);
			archive_write_data(a, xmlData.data(), xmlData.size());

			// Free the entry
			archive_entry_free(entry);
			streamData.str("");
		}

		g_gui.SetLoadDone(0, "Saving OTBM map...");

		MemoryNodeFileWriteHandle otbmWriter;
		saveMap(map, otbmWriter);

		g_gui.SetLoadDone(75, "Compressing...");

		// Create an archive entry for the otbm file
		entry = archive_entry_new();
		archive_entry_set_pathname(entry, "world/map.otbm");
		archive_entry_set_size(entry, otbmWriter.getSize() + 4); // 4 bytes extra for header
		archive_entry_set_filetype(entry, AE_IFREG);
		archive_entry_set_perm(entry, 0644);
		archive_write_header(a, entry);

		// Write the version header
		char otbm_identifier[] = "OTBM";
		archive_write_data(a, otbm_identifier, 4);

		// Write the OTBM data
		archive_write_data(a, otbmWriter.getMemory(), otbmWriter.getSize());
		archive_entry_free(entry);

		// Free / close the archive
		archive_write_close(a);
		archive_write_free(a);

		g_gui.DestroyLoadBar();
		return true;
	}
#endif

	DiskNodeFileWriteHandle f(
		nstr(identifier.GetFullPath()),
		(g_settings.getInteger(Config::SAVE_WITH_OTB_MAGIC_NUMBER) ? "OTBM" : std::string(4, '\0'))
		);

	if(!f.isOk()) {
		error("Can not open file %s for writing", (const char*)identifier.GetFullPath().mb_str(wxConvUTF8));
		return false;
	}

	if(!saveMap(map, f))
		return false;

	g_gui.SetLoadDone(99, "Saving spawns...");
	saveSpawns(map, identifier);

	g_gui.SetLoadDone(99, "Saving houses...");
	saveHouses(map, identifier);

	saveZonesToToml(zoneMap, identifier, map);

	return true;
}

bool IOMapOTBM::saveMap(Map& map, NodeFileWriteHandle& f)
{
	/* STOP!
	 * Before you even think about modifying this, please reconsider.
	 * while adding stuff to the binary format may be "cool", you'll
	 * inevitably make it incompatible with any future releases of
	 * the map editor, meaning you cannot reuse your map. Before you
	 * try to modify this, PLEASE consider using an external file
	 * like spawns.xml or houses.xml, as that will be MUCH easier
	 * to port to newer versions of the editor than a custom binary
	 * format.
	 */

	const IOMapOTBM& self = *this;

	FileName tmpName;
	MapVersion mapVersion = map.getVersion();

	f.addNode(0);
	{
		f.addU32(mapVersion.otbm); // Version

		f.addU16(map.width);
		f.addU16(map.height);

		f.addU32(g_items.MajorVersion);
		f.addU32(g_items.MinorVersion);

		f.addNode(OTBM_MAP_DATA);
		{
			f.addByte(OTBM_ATTR_DESCRIPTION);
			// Neither SimOne's nor OpenTibia cares for additional description tags
			f.addString("Saved with Remere's Map Editor " + __RME_VERSION__);

			f.addU8(OTBM_ATTR_DESCRIPTION);
			f.addString(map.description);

			tmpName.Assign(wxstr(map.spawnfile));
			f.addU8(OTBM_ATTR_EXT_SPAWN_FILE);
			f.addString(nstr(tmpName.GetFullName()));

			tmpName.Assign(wxstr(map.housefile));
			f.addU8(OTBM_ATTR_EXT_HOUSE_FILE);
			f.addString(nstr(tmpName.GetFullName()));

			// Start writing tiles
			uint32_t tiles_saved = 0;
			bool first = true;

			int local_x = -1, local_y = -1, local_z = -1;

			MapIterator map_iterator = map.begin();
			while(map_iterator != map.end()) {
				// Update progressbar
				++tiles_saved;
				if(tiles_saved % 8192 == 0)
					g_gui.SetLoadDone(int(tiles_saved / double(map.getTileCount()) * 100.0));

				// Get tile
				Tile* save_tile = (*map_iterator)->get();

				// Is it an empty tile that we can skip? (Leftovers...)
				if(!save_tile || save_tile->size() == 0) {
					++map_iterator;
					continue;
				}

				const Position& pos = save_tile->getPosition();

				// Decide if newd node should be created
				if(pos.x < local_x || pos.x >= local_x + 256 || pos.y < local_y || pos.y >= local_y + 256 || pos.z != local_z) {
					// End last node
					if(!first) {
						f.endNode();
					}
					first = false;

					// Start newd node
					f.addNode(OTBM_TILE_AREA);
					f.addU16(local_x = pos.x & 0xFF00);
					f.addU16(local_y = pos.y & 0xFF00);
					f.addU8( local_z = pos.z);
				}
				f.addNode(save_tile->isHouseTile()? OTBM_HOUSETILE : OTBM_TILE);

				f.addU8(save_tile->getX() & 0xFF);
				f.addU8(save_tile->getY() & 0xFF);

				if(save_tile->isHouseTile()) {
					f.addU32(save_tile->getHouseID());
				}

				if(save_tile->getMapFlags()) {
					f.addByte(OTBM_ATTR_TILE_FLAGS);
					f.addU32(save_tile->getMapFlags());
					if (save_tile->getMapFlags() & TILESTATE_ZONE_BRUSH)
					{
						for (const auto& zoneId : save_tile->getZoneIds())
						{
							zoneMap[zoneId].push_back(save_tile->getPosition());
						}
					}
				}

				if(save_tile->ground) {
					Item* ground = save_tile->ground;
					if(ground->isMetaItem()) {
						// Do nothing, we don't save metaitems...
					} else if(ground->hasBorderEquivalent()) {
						bool found = false;
						for(Item* item : save_tile->items) {
							if(item->getGroundEquivalent() == ground->getID()) {
								// Do nothing
								// Found equivalent
								found = true;
								break;
							}
						}

						if(!found) {
							ground->serializeItemNode_OTBM(self, f);
						}
					} else if(ground->isComplex()) {
						ground->serializeItemNode_OTBM(self, f);
					} else {
						f.addByte(OTBM_ATTR_ITEM);
						ground->serializeItemCompact_OTBM(self, f);
					}
				}

				for(Item* item : save_tile->items) {
					if(!item->isMetaItem()) {
						item->serializeItemNode_OTBM(self, f);
					}
				}

				f.endNode();
				++map_iterator;
			}

			// Only close the last node if one has actually been created
			if(!first) {
				f.endNode();
			}

			f.addNode(OTBM_TOWNS);
			for(const auto& townEntry : map.towns) {
				Town* town = townEntry.second;
				const Position& townPosition = town->getTemplePosition();
				f.addNode(OTBM_TOWN);
					f.addU32(town->getID());
					f.addString(town->getName());
					f.addU16(townPosition.x);
					f.addU16(townPosition.y);
					f.addU8(townPosition.z);
				f.endNode();
			}
			f.endNode();

			if(version.otbm >= MAP_OTBM_3) {
				f.addNode(OTBM_WAYPOINTS);
				for(const auto& waypointEntry : map.waypoints) {
					Waypoint* waypoint = waypointEntry.second;
					f.addNode(OTBM_WAYPOINT);
						f.addString(waypoint->name);
						f.addU16(waypoint->pos.x);
						f.addU16(waypoint->pos.y);
						f.addU8(waypoint->pos.z);
					f.endNode();
				}
				f.endNode();
			}
		}
		f.endNode();
	}
	f.endNode();
	return true;
}

bool IOMapOTBM::saveSpawns(Map& map, const FileName& dir)
{
	wxString filepath = dir.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME);
	filepath += wxString(map.spawnfile.c_str(), wxConvUTF8);

	// Create the XML file
	pugi::xml_document doc;
	if(saveSpawns(map, doc)) {
		return doc.save_file(filepath.wc_str(), "\t", pugi::format_default, pugi::encoding_utf8);
	}
	return false;
}

bool IOMapOTBM::saveSpawns(Map& map, pugi::xml_document& doc)
{
	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	if(!decl) {
		return false;
	}

	decl.append_attribute("version") = "1.0";

	CreatureList creatureList;

	pugi::xml_node spawnNodes = doc.append_child("spawns");
	for(const auto& spawnPosition : map.spawns) {
		Tile *tile = map.getTile(spawnPosition);
		if(tile == nullptr)
			continue;

		Spawn* spawn = tile->spawn;
		ASSERT(spawn);

		pugi::xml_node spawnNode = spawnNodes.append_child("spawn");

		spawnNode.append_attribute("centerx") = spawnPosition.x;
		spawnNode.append_attribute("centery") = spawnPosition.y;
		spawnNode.append_attribute("centerz") = spawnPosition.z;

		int32_t radius = spawn->getSize();
		spawnNode.append_attribute("radius") = radius;

		for(int32_t y = -radius; y <= radius; ++y) {
			for(int32_t x = -radius; x <= radius; ++x) {
				Tile* creature_tile = map.getTile(spawnPosition + Position(x, y, 0));
				if(creature_tile) {
					Creature* creature = creature_tile->creature;
					if(creature && !creature->isSaved()) {
						pugi::xml_node creatureNode = spawnNode.append_child(creature->isNpc() ? "npc" : "monster");

						creatureNode.append_attribute("name") = creature->getName().c_str();
						creatureNode.append_attribute("x") = x;
						creatureNode.append_attribute("y") = y;
						creatureNode.append_attribute("z") = spawnPosition.z;
						creatureNode.append_attribute("spawntime") = creature->getSpawnTime();
						creatureNode.append_attribute("direction") = creature->getDirection();


						// Mark as saved
						creature->save();
						creatureList.push_back(creature);
					}
				}
			}
		}
	}

	for(Creature* creature : creatureList) {
		creature->reset();
	}
	return true;
}

bool IOMapOTBM::saveHouses(Map& map, const FileName& dir)
{
	wxString filepath = dir.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME);
	filepath += wxString(map.housefile.c_str(), wxConvUTF8);

	// Create the XML file
	pugi::xml_document doc;
	if(saveHouses(map, doc)) {
		return doc.save_file(filepath.wc_str(), "\t", pugi::format_default, pugi::encoding_utf8);
	}
	return false;
}

bool IOMapOTBM::saveHouses(Map& map, pugi::xml_document& doc)
{
	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	if(!decl) {
		return false;
	}

	decl.append_attribute("version") = "1.0";

	pugi::xml_node houseNodes = doc.append_child("houses");
	for(const auto& houseEntry : map.houses) {
		const House* house = houseEntry.second;
		pugi::xml_node houseNode = houseNodes.append_child("house");

		houseNode.append_attribute("name") = house->name.c_str();
		houseNode.append_attribute("houseid") = house->id;

		const Position& exitPosition = house->getExit();
		houseNode.append_attribute("entryx") = exitPosition.x;
		houseNode.append_attribute("entryy") = exitPosition.y;
		houseNode.append_attribute("entryz") = exitPosition.z;

		houseNode.append_attribute("rent") = house->rent;
		if(house->guildhall) {
			houseNode.append_attribute("guildhall") = true;
		}

		houseNode.append_attribute("townid") = house->townid;
		houseNode.append_attribute("size") = static_cast<int32_t>(house->size());
	}
	return true;
}
