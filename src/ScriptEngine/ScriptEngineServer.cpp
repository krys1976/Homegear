/* Copyright 2013-2017 Sathya Laufer
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Homegear.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU Lesser General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
*/

#ifndef NO_SCRIPTENGINE

#include "ScriptEngineServer.h"
#include "../GD/GD.h"
#include <homegear-base/BaseLib.h>

// Use e. g. for debugging with valgrind. Note that only one client can be started if activated.
#define MANUAL_CLIENT_START

namespace ScriptEngine
{

ScriptEngineServer::ScriptEngineServer() : IQueue(GD::bl.get(), 2, 1000)
{
	_out.init(GD::bl.get());
	_out.setPrefix("Script Engine Server: ");

#ifdef DEBUGSESOCKET
	BaseLib::Io::deleteFile(GD::bl->settings.logfilePath() + "homegear-socket.pcap");
	_socketOutput.open(GD::bl->settings.logfilePath() + "homegear-socket.pcap", std::ios::app | std::ios::binary);
	std::vector<uint8_t> buffer{ 0xa1, 0xb2, 0xc3, 0xd4, 0, 2, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0x7F, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0xe4 };
	_socketOutput.write((char*)buffer.data(), buffer.size());
#endif

	_shuttingDown = false;
	_stopServer = false;

	_rpcDecoder = std::unique_ptr<BaseLib::Rpc::RpcDecoder>(new BaseLib::Rpc::RpcDecoder(GD::bl.get(), false, true));
	_rpcEncoder = std::unique_ptr<BaseLib::Rpc::RpcEncoder>(new BaseLib::Rpc::RpcEncoder(GD::bl.get(), true));
	_dummyClientInfo.reset(new BaseLib::RpcClientInfo());

	_rpcMethods.emplace("devTest", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCDevTest()));
	_rpcMethods.emplace("system.getCapabilities", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSystemGetCapabilities()));
	_rpcMethods.emplace("system.listMethods", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSystemListMethods(GD::rpcServers[0].getServer())));
	_rpcMethods.emplace("system.methodHelp", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSystemMethodHelp(GD::rpcServers[0].getServer())));
	_rpcMethods.emplace("system.methodSignature", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSystemMethodSignature(GD::rpcServers[0].getServer())));
	_rpcMethods.emplace("system.multicall", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSystemMulticall(GD::rpcServers[0].getServer())));
	_rpcMethods.emplace("activateLinkParamset", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCActivateLinkParamset()));
	_rpcMethods.emplace("abortEventReset", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCTriggerEvent()));
	_rpcMethods.emplace("addDevice", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCAddDevice()));
	_rpcMethods.emplace("addEvent", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCAddEvent()));
	_rpcMethods.emplace("addLink", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCAddLink()));
	_rpcMethods.emplace("copyConfig", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCCopyConfig()));
	_rpcMethods.emplace("clientServerInitialized", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCClientServerInitialized()));
	_rpcMethods.emplace("createDevice", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCCreateDevice()));
	_rpcMethods.emplace("deleteData", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCDeleteData()));
	_rpcMethods.emplace("deleteDevice", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCDeleteDevice()));
	_rpcMethods.emplace("deleteMetadata", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCDeleteMetadata()));
	_rpcMethods.emplace("deleteNodeData", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCDeleteNodeData()));
	_rpcMethods.emplace("deleteSystemVariable", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCDeleteSystemVariable()));
	_rpcMethods.emplace("enableEvent", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCEnableEvent()));
	_rpcMethods.emplace("getAllConfig", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetAllConfig()));
	_rpcMethods.emplace("getAllMetadata", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetAllMetadata()));
	_rpcMethods.emplace("getAllScripts", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetAllScripts()));
	_rpcMethods.emplace("getAllSystemVariables", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetAllSystemVariables()));
	_rpcMethods.emplace("getAllValues", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetAllValues()));
	_rpcMethods.emplace("getConfigParameter", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetConfigParameter()));
	_rpcMethods.emplace("getData", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetData()));
	_rpcMethods.emplace("getDeviceDescription", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetDeviceDescription()));
	_rpcMethods.emplace("getDeviceInfo", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetDeviceInfo()));
	_rpcMethods.emplace("getEvent", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetEvent()));
	_rpcMethods.emplace("getInstallMode", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetInstallMode()));
	_rpcMethods.emplace("getKeyMismatchDevice", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetKeyMismatchDevice()));
	_rpcMethods.emplace("getLinkInfo", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetLinkInfo()));
	_rpcMethods.emplace("getLinkPeers", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetLinkPeers()));
	_rpcMethods.emplace("getLinks", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetLinks()));
	_rpcMethods.emplace("getMetadata", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetMetadata()));
	_rpcMethods.emplace("getName", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetName()));
	_rpcMethods.emplace("getNodeData", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetNodeData()));
	_rpcMethods.emplace("getPairingMethods", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetPairingMethods()));
	_rpcMethods.emplace("getParamset", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetParamset()));
	_rpcMethods.emplace("getParamsetDescription", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetParamsetDescription()));
	_rpcMethods.emplace("getParamsetId", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetParamsetId()));
	_rpcMethods.emplace("getPeerId", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetPeerId()));
	_rpcMethods.emplace("getServiceMessages", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetServiceMessages()));
	_rpcMethods.emplace("getSystemVariable", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetSystemVariable()));
	_rpcMethods.emplace("getUpdateStatus", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetUpdateStatus()));
	_rpcMethods.emplace("getValue", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetValue()));
	_rpcMethods.emplace("getVariableDescription", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetVariableDescription()));
	_rpcMethods.emplace("getVersion", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCGetVersion()));
	_rpcMethods.emplace("init", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCInit()));
	_rpcMethods.emplace("listBidcosInterfaces", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCListBidcosInterfaces()));
	_rpcMethods.emplace("listClientServers", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCListClientServers()));
	_rpcMethods.emplace("listDevices", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCListDevices()));
	_rpcMethods.emplace("listEvents", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCListEvents()));
	_rpcMethods.emplace("listFamilies", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCListFamilies()));
	_rpcMethods.emplace("listInterfaces", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCListInterfaces()));
	_rpcMethods.emplace("listKnownDeviceTypes", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCListKnownDeviceTypes()));
	_rpcMethods.emplace("listTeams", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCListTeams()));
	_rpcMethods.emplace("logLevel", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCLogLevel()));
	_rpcMethods.emplace("ping", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCPing()));
	_rpcMethods.emplace("putParamset", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCPutParamset()));
	_rpcMethods.emplace("removeEvent", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCRemoveEvent()));
	_rpcMethods.emplace("removeLink", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCRemoveLink()));
	_rpcMethods.emplace("reportValueUsage", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCReportValueUsage()));
	_rpcMethods.emplace("rssiInfo", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCRssiInfo()));
	_rpcMethods.emplace("runScript", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCRunScript()));
	_rpcMethods.emplace("searchDevices", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSearchDevices()));
	_rpcMethods.emplace("setData", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSetData()));
	_rpcMethods.emplace("setId", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSetId()));
	_rpcMethods.emplace("setInstallMode", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSetInstallMode()));
	_rpcMethods.emplace("setInterface", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSetInterface()));
	_rpcMethods.emplace("setLinkInfo", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSetLinkInfo()));
	_rpcMethods.emplace("setMetadata", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSetMetadata()));
	_rpcMethods.emplace("setName", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSetName()));
	_rpcMethods.emplace("setNodeData", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSetNodeData()));
	_rpcMethods.emplace("setNodeVariable", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSetNodeVariable()));
	_rpcMethods.emplace("setSystemVariable", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSetSystemVariable()));
	_rpcMethods.emplace("setTeam", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSetTeam()));
	_rpcMethods.emplace("setValue", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSetValue()));
	_rpcMethods.emplace("subscribePeers", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCSubscribePeers()));
	_rpcMethods.emplace("triggerEvent", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCTriggerEvent()));
	_rpcMethods.emplace("triggerRpcEvent", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCTriggerRpcEvent()));
	_rpcMethods.emplace("unsubscribePeers", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCUnsubscribePeers()));
	_rpcMethods.emplace("updateFirmware", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCUpdateFirmware()));
	_rpcMethods.emplace("writeLog", std::shared_ptr<BaseLib::Rpc::RpcMethod>(new Rpc::RPCWriteLog()));

	_localRpcMethods.emplace("registerScriptEngineClient", std::bind(&ScriptEngineServer::registerScriptEngineClient, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("scriptFinished", std::bind(&ScriptEngineServer::scriptFinished, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("scriptOutput", std::bind(&ScriptEngineServer::scriptOutput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("scriptHeaders", std::bind(&ScriptEngineServer::scriptHeaders, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("peerExists", std::bind(&ScriptEngineServer::peerExists, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	_localRpcMethods.emplace("listRpcClients", std::bind(&ScriptEngineServer::listRpcClients, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("raiseDeleteDevice", std::bind(&ScriptEngineServer::raiseDeleteDevice, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("getFamilySetting", std::bind(&ScriptEngineServer::getFamilySetting, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("setFamilySetting", std::bind(&ScriptEngineServer::setFamilySetting, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("deleteFamilySetting", std::bind(&ScriptEngineServer::deleteFamilySetting, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	_localRpcMethods.emplace("mqttPublish", std::bind(&ScriptEngineServer::mqttPublish, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	_localRpcMethods.emplace("auth", std::bind(&ScriptEngineServer::auth, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("createUser", std::bind(&ScriptEngineServer::createUser, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("deleteUser", std::bind(&ScriptEngineServer::deleteUser, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("updateUser", std::bind(&ScriptEngineServer::updateUser, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("userExists", std::bind(&ScriptEngineServer::userExists, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("listUsers", std::bind(&ScriptEngineServer::listUsers, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	_localRpcMethods.emplace("listModules", std::bind(&ScriptEngineServer::listModules, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("loadModule", std::bind(&ScriptEngineServer::loadModule, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("unloadModule", std::bind(&ScriptEngineServer::unloadModule, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("reloadModule", std::bind(&ScriptEngineServer::reloadModule, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	_localRpcMethods.emplace("checkLicense", std::bind(&ScriptEngineServer::checkLicense, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("removeLicense", std::bind(&ScriptEngineServer::removeLicense, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("getLicenseStates", std::bind(&ScriptEngineServer::getLicenseStates, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("getTrialStartTime", std::bind(&ScriptEngineServer::getTrialStartTime, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	_localRpcMethods.emplace("nodeOutput", std::bind(&ScriptEngineServer::nodeOutput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_localRpcMethods.emplace("executePhpNodeBaseMethod", std::bind(&ScriptEngineServer::executePhpNodeBaseMethod, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

ScriptEngineServer::~ScriptEngineServer()
{
	if(!_stopServer) stop();
	GD::bl->threadManager.join(_scriptFinishedThread);
#ifdef DEBUGSESOCKET
	_socketOutput.close();
#endif
}

#ifdef DEBUGSESOCKET
void ScriptEngineServer::socketOutput(int32_t packetId, PScriptEngineClientData& clientData, bool serverRequest, bool request, std::vector<char> data)
{
	try
	{
		int64_t time = BaseLib::HelperFunctions::getTimeMicroseconds();
		int32_t timeSeconds = time / 1000000;
		int32_t timeMicroseconds = time % 1000000;

		uint32_t length = 20 + 8 + data.size();

		std::vector<uint8_t> buffer;
		buffer.reserve(length);
		buffer.push_back((uint8_t)(timeSeconds >> 24));
		buffer.push_back((uint8_t)(timeSeconds >> 16));
		buffer.push_back((uint8_t)(timeSeconds >> 8));
		buffer.push_back((uint8_t)timeSeconds);

		buffer.push_back((uint8_t)(timeMicroseconds >> 24));
		buffer.push_back((uint8_t)(timeMicroseconds >> 16));
		buffer.push_back((uint8_t)(timeMicroseconds >> 8));
		buffer.push_back((uint8_t)timeMicroseconds);

		buffer.push_back((uint8_t)(length >> 24)); //incl_len
		buffer.push_back((uint8_t)(length >> 16));
		buffer.push_back((uint8_t)(length >> 8));
		buffer.push_back((uint8_t)length);

		buffer.push_back((uint8_t)(length >> 24)); //orig_len
		buffer.push_back((uint8_t)(length >> 16));
		buffer.push_back((uint8_t)(length >> 8));
		buffer.push_back((uint8_t)length);

		//{{{ IPv4 header
			buffer.push_back(0x45); //Version 4 (0100....); Header length 20 (....0101)
			buffer.push_back(0); //Differentiated Services Field

			buffer.push_back((uint8_t)(length >> 8)); //Length
			buffer.push_back((uint8_t)length);

			buffer.push_back((uint8_t)((packetId % 65536) >> 8)); //Identification
			buffer.push_back((uint8_t)(packetId % 65536));

			buffer.push_back(0); //Flags: 0 (000.....); Fragment offset 0 (...00000 00000000)
			buffer.push_back(0);

			buffer.push_back(0x80); //TTL

			buffer.push_back(17); //Protocol UDP

			buffer.push_back(0); //Header checksum
			buffer.push_back(0);

			if(request)
			{
				if(serverRequest)
				{
					buffer.push_back(1); //Source
					buffer.push_back(1);
					buffer.push_back(1);
					buffer.push_back(1);

					buffer.push_back(0x80 | (uint8_t)(clientData->pid >> 24)); //Destination
					buffer.push_back((uint8_t)(clientData->pid >> 16));
					buffer.push_back((uint8_t)(clientData->pid >> 8));
					buffer.push_back((uint8_t)clientData->pid);
				}
				else
				{
					buffer.push_back(0x80 | (uint8_t)(clientData->pid >> 24)); //Source
					buffer.push_back((uint8_t)(clientData->pid >> 16));
					buffer.push_back((uint8_t)(clientData->pid >> 8));
					buffer.push_back((uint8_t)clientData->pid);

					buffer.push_back(2); //Destination
					buffer.push_back(2);
					buffer.push_back(2);
					buffer.push_back(2);
				}
			}
			else
			{
				if(serverRequest)
				{
					buffer.push_back(0x80 | (uint8_t)(clientData->pid >> 24)); //Source
					buffer.push_back((uint8_t)(clientData->pid >> 16));
					buffer.push_back((uint8_t)(clientData->pid >> 8));
					buffer.push_back((uint8_t)clientData->pid);

					buffer.push_back(1); //Destination
					buffer.push_back(1);
					buffer.push_back(1);
					buffer.push_back(1);
				}
				else
				{
					buffer.push_back(2); //Source
					buffer.push_back(2);
					buffer.push_back(2);
					buffer.push_back(2);

					buffer.push_back(0x80 | (uint8_t)(clientData->pid >> 24)); //Destination
					buffer.push_back((uint8_t)(clientData->pid >> 16));
					buffer.push_back((uint8_t)(clientData->pid >> 8));
					buffer.push_back((uint8_t)clientData->pid);
				}
			}
		// }}}
		// {{{ UDP header
			buffer.push_back(0); //Source port
			buffer.push_back(1);

			buffer.push_back((uint8_t)(clientData->pid >> 8)); //Destination port
			buffer.push_back((uint8_t)clientData->pid);

			length -= 20;
			buffer.push_back((uint8_t)(length >> 8)); //Length
			buffer.push_back((uint8_t)length);

			buffer.push_back(0); //Checksum
			buffer.push_back(0);
		// }}}

		buffer.insert(buffer.end(), data.begin(), data.end());

		std::lock_guard<std::mutex> socketOutputGuard(_socketOutputMutex);
		_socketOutput.write((char*)buffer.data(), buffer.size());
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}
#endif

void ScriptEngineServer::collectGarbage()
{
	try
	{
		_lastGargabeCollection = GD::bl->hf.getTime();
		std::vector<PScriptEngineProcess> processesToShutdown;
		{
			std::lock_guard<std::mutex> processGuard(_processMutex);
			bool emptyProcess = false;
			bool emptyNodeProcess = false;
			for(std::map<pid_t, PScriptEngineProcess>::iterator i = _processes.begin(); i != _processes.end(); ++i)
			{
				if(i->second->scriptCount() == 0 && i->second->getClientData() && !i->second->getClientData()->closed)
				{
					if(i->second->isNodeProcess())
					{
						if(emptyNodeProcess) closeClientConnection(i->second->getClientData());
						else emptyNodeProcess = true;
					}
					else
					{
						if(emptyProcess) closeClientConnection(i->second->getClientData());
						else emptyProcess = true;
					}
				}
			}
		}

		std::vector<PScriptEngineClientData> clientsToRemove;
		{
			std::lock_guard<std::mutex> stateGuard(_stateMutex);
			try
			{
				for(std::map<int32_t, PScriptEngineClientData>::iterator i = _clients.begin(); i != _clients.end(); ++i)
				{
					if(i->second->closed) clientsToRemove.push_back(i->second);
				}
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
		}
		for(std::vector<PScriptEngineClientData>::iterator i = clientsToRemove.begin(); i != clientsToRemove.end(); ++i)
		{
			{
				std::lock_guard<std::mutex> stateGuard(_stateMutex);
				_clients.erase((*i)->id);
			}
			_out.printDebug("Debug: Script engine client " + std::to_string((*i)->id) + " removed.");
		}
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

bool ScriptEngineServer::start()
{
	try
	{
		stop();
		_socketPath = GD::bl->settings.socketPath() + "homegearSE.sock";
		_shuttingDown = false;
		_stopServer = false;
		if(!getFileDescriptor(true)) return false;
		uint32_t scriptEngineThreadCount = GD::bl->settings.scriptEngineThreadCount();
		if(scriptEngineThreadCount < 5) scriptEngineThreadCount = 5;
		startQueue(0, false, scriptEngineThreadCount, 0, SCHED_OTHER);
		startQueue(1, false, scriptEngineThreadCount, 0, SCHED_OTHER);
		GD::bl->threadManager.start(_mainThread, true, &ScriptEngineServer::mainThread, this);
		return true;
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return false;
}

void ScriptEngineServer::stop()
{
	try
	{
		_shuttingDown = true;
		_out.printDebug("Debug: Waiting for script engine server's client threads to finish.");
		std::vector<PScriptEngineClientData> clients;
		{
			std::lock_guard<std::mutex> stateGuard(_stateMutex);
			for(std::map<int32_t, PScriptEngineClientData>::iterator i = _clients.begin(); i != _clients.end(); ++i)
			{
				clients.push_back(i->second);
				closeClientConnection(i->second);
				std::lock_guard<std::mutex> processGuard(_processMutex);
				std::map<pid_t, std::shared_ptr<ScriptEngineProcess>>::iterator processIterator = _processes.find(i->second->pid);
				if(processIterator != _processes.end())
				{
					processIterator->second->invokeScriptFinished(-32501);
					_processes.erase(processIterator);
				}
			}
		}

		_stopServer = true;
		GD::bl->threadManager.join(_mainThread);
		while(_clients.size() > 0)
		{
			for(std::vector<PScriptEngineClientData>::iterator i = clients.begin(); i != clients.end(); ++i)
			{
				(*i)->requestConditionVariable.notify_all();
			}
			collectGarbage();
			if(_clients.size() > 0) std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		stopQueue(0);
		stopQueue(1);
		unlink(_socketPath.c_str());
	}
	catch(const std::exception& ex)
	{
		_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
}

void ScriptEngineServer::homegearShuttingDown()
{
	try
	{
		_shuttingDown = true;
		std::vector<PScriptEngineClientData> clients;
		{
			std::lock_guard<std::mutex> stateGuard(_stateMutex);
			for(std::map<int32_t, PScriptEngineClientData>::iterator i = _clients.begin(); i != _clients.end(); ++i)
			{
				if(i->second->closed) continue;
				clients.push_back(i->second);
			}
		}
		for(std::vector<PScriptEngineClientData>::iterator i = clients.begin(); i != clients.end(); ++i)
		{
			BaseLib::PArray parameters(new BaseLib::Array());
			sendRequest(*i, "shutdown", parameters);
		}

		int32_t i = 0;
		while(_clients.size() > 0 && i < 60)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			collectGarbage();
			i++;
		}
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void ScriptEngineServer::homegearReloading()
{
	try
	{
		std::vector<PScriptEngineClientData> clients;
		{
			std::lock_guard<std::mutex> stateGuard(_stateMutex);
			for(std::map<int32_t, PScriptEngineClientData>::iterator i = _clients.begin(); i != _clients.end(); ++i)
			{
				if(i->second->closed) continue;
				clients.push_back(i->second);
			}
		}
		for(std::vector<PScriptEngineClientData>::iterator i = clients.begin(); i != clients.end(); ++i)
		{
			BaseLib::PArray parameters(new BaseLib::Array());
			sendRequest(*i, "reload", parameters);
		}
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void ScriptEngineServer::processKilled(pid_t pid, int32_t exitCode, int32_t signal, bool coreDumped)
{
	try
	{
		std::shared_ptr<ScriptEngineProcess> process;

		{
			std::lock_guard<std::mutex> processGuard(_processMutex);
			std::map<pid_t, std::shared_ptr<ScriptEngineProcess>>::iterator processIterator = _processes.find(pid);
			if(processIterator != _processes.end())
			{
				process = processIterator->second;
				closeClientConnection(process->getClientData());
				_processes.erase(processIterator);
			}
		}

		if(process)
		{
			if(signal != -1 && signal != 15) _out.printCritical("Critical: Client process with pid " + std::to_string(pid) + " was killed with signal " + std::to_string(signal) + '.');
			else if(exitCode != 0) _out.printError("Error: Client process with pid " + std::to_string(pid) + " exited with code " + std::to_string(exitCode) + '.');
			else _out.printInfo("Info: Client process with pid " + std::to_string(pid) + " exited with code " + std::to_string(exitCode) + '.');

			if(signal != -1) exitCode = -32500;

			{
				std::lock_guard<std::mutex> scriptFinishedGuard(_scriptFinishedThreadMutex);
				GD::bl->threadManager.start(_scriptFinishedThread, true, &ScriptEngineServer::invokeScriptFinished, this, process, -1, exitCode);
			}
		}
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void ScriptEngineServer::devTestClient()
{
	try
	{
		if(_shuttingDown) return;
		std::vector<PScriptEngineClientData> clients;
		{
			std::lock_guard<std::mutex> stateGuard(_stateMutex);
			for(std::map<int32_t, PScriptEngineClientData>::iterator i = _clients.begin(); i != _clients.end(); ++i)
			{
				if(i->second->closed) continue;
				clients.push_back(i->second);
			}
		}

		BaseLib::Array parameterArray{ std::make_shared<BaseLib::Variable>(6) };
		BaseLib::PArray parameters = std::make_shared<BaseLib::Array>(std::move(parameterArray));
		for(std::vector<PScriptEngineClientData>::iterator i = clients.begin(); i != clients.end(); ++i)
		{
			BaseLib::PVariable response = sendRequest(*i, "devTest", parameters);
		}
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

uint32_t ScriptEngineServer::scriptCount()
{
	try
	{
		if(_shuttingDown) return 0;
		std::vector<PScriptEngineClientData> clients;
		{
			std::lock_guard<std::mutex> stateGuard(_stateMutex);
			for(std::map<int32_t, PScriptEngineClientData>::iterator i = _clients.begin(); i != _clients.end(); ++i)
			{
				if(i->second->closed) continue;
				clients.push_back(i->second);
			}
		}

		uint32_t count = 0;
		BaseLib::PArray parameters = std::make_shared<BaseLib::Array>();
		for(std::vector<PScriptEngineClientData>::iterator i = clients.begin(); i != clients.end(); ++i)
		{
			BaseLib::PVariable response = sendRequest(*i, "scriptCount", parameters);
			count += response->integerValue;
		}
		return count;
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return 0;
}

std::vector<std::tuple<int32_t, uint64_t, int32_t, std::string>> ScriptEngineServer::getRunningScripts()
{
	try
	{
		if(_shuttingDown) return std::vector<std::tuple<int32_t, uint64_t, int32_t, std::string>>();
		std::vector<PScriptEngineClientData> clients;
		{
			std::lock_guard<std::mutex> stateGuard(_stateMutex);
			for(std::map<int32_t, PScriptEngineClientData>::iterator i = _clients.begin(); i != _clients.end(); ++i)
			{
				if(i->second->closed) continue;
				clients.push_back(i->second);
			}
		}

		std::vector<std::tuple<int32_t, uint64_t, int32_t, std::string>> runningScripts;
		BaseLib::PArray parameters = std::make_shared<BaseLib::Array>();
		for(std::vector<PScriptEngineClientData>::iterator i = clients.begin(); i != clients.end(); ++i)
		{
			BaseLib::PVariable response = sendRequest(*i, "getRunningScripts", parameters);
			if(runningScripts.capacity() <= runningScripts.size() + response->arrayValue->size()) runningScripts.reserve(runningScripts.capacity() + response->arrayValue->size() + 100);
			for(auto& script : *(response->arrayValue))
			{
				runningScripts.push_back(std::tuple<int32_t, uint64_t, int32_t, std::string>((int32_t)(*i)->pid, script->arrayValue->at(0)->integerValue64, script->arrayValue->at(1)->integerValue, script->arrayValue->at(2)->stringValue));
			}
		}
		return runningScripts;
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return std::vector<std::tuple<int32_t, uint64_t, int32_t, std::string>>();
}

BaseLib::PVariable ScriptEngineServer::executePhpNodeMethod(BaseLib::PArray& parameters)
{
	try
	{
		if(parameters->size() != 3) return BaseLib::Variable::createError(-1, "Method expects exactly three parameters.");

		PScriptEngineClientData clientData;
		int32_t clientId = 0;
		{
			std::lock_guard<std::mutex> nodeClientIdMapGuard(_nodeClientIdMapMutex);
			auto nodeClientIdIterator = _nodeClientIdMap.find(parameters->at(0)->stringValue);
			if(nodeClientIdIterator == _nodeClientIdMap.end()) return BaseLib::Variable::createError(-1, "Unknown node.");
			clientId = nodeClientIdIterator->second;
		}
		{
			std::lock_guard<std::mutex> stateGuard(_stateMutex);
			auto clientIterator = _clients.find(clientId);
			if(clientIterator == _clients.end()) return BaseLib::Variable::createError(-32501, "Node process not found.");
			clientData = clientIterator->second;
		}

		return sendRequest(clientData, "executePhpNodeMethod", parameters);
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return BaseLib::Variable::createError(-32500, "Unknown application error.");
}

void ScriptEngineServer::broadcastEvent(uint64_t id, int32_t channel, std::shared_ptr<std::vector<std::string>> variables, BaseLib::PArray values)
{
	try
	{
		if(_shuttingDown) return;
		std::vector<PScriptEngineClientData> clients;
		{
			std::lock_guard<std::mutex> stateGuard(_stateMutex);
			for(std::map<int32_t, PScriptEngineClientData>::iterator i = _clients.begin(); i != _clients.end(); ++i)
			{
				if(i->second->closed) continue;
				clients.push_back(i->second);
			}
		}

		for(std::vector<PScriptEngineClientData>::iterator i = clients.begin(); i != clients.end(); ++i)
		{
			BaseLib::PArray parameters(new BaseLib::Array{BaseLib::PVariable(new BaseLib::Variable(id)), BaseLib::PVariable(new BaseLib::Variable(channel)), BaseLib::PVariable(new BaseLib::Variable(*variables)), BaseLib::PVariable(new BaseLib::Variable(values))});
			std::shared_ptr<BaseLib::IQueueEntry> queueEntry = std::make_shared<QueueEntry>(*i, "broadcastEvent", parameters);
			enqueue(1, queueEntry);
		}
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void ScriptEngineServer::broadcastNewDevices(BaseLib::PVariable deviceDescriptions)
{
	try
	{
		if(_shuttingDown) return;
		std::vector<PScriptEngineClientData> clients;
		{
			std::lock_guard<std::mutex> stateGuard(_stateMutex);
			for(std::map<int32_t, PScriptEngineClientData>::iterator i = _clients.begin(); i != _clients.end(); ++i)
			{
				if(i->second->closed) continue;
				clients.push_back(i->second);
			}
		}

		for(std::vector<PScriptEngineClientData>::iterator i = clients.begin(); i != clients.end(); ++i)
		{
			BaseLib::PArray parameters(new BaseLib::Array{deviceDescriptions});
			std::shared_ptr<BaseLib::IQueueEntry> queueEntry = std::make_shared<QueueEntry>(*i, "broadcastNewDevices", parameters);
			enqueue(1, queueEntry);
		}
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void ScriptEngineServer::broadcastDeleteDevices(BaseLib::PVariable deviceInfo)
{
	try
	{
		if(_shuttingDown) return;
		std::vector<PScriptEngineClientData> clients;
		{
			std::lock_guard<std::mutex> stateGuard(_stateMutex);
			for(std::map<int32_t, PScriptEngineClientData>::iterator i = _clients.begin(); i != _clients.end(); ++i)
			{
				if(i->second->closed) continue;
				clients.push_back(i->second);
			}
		}

		for(std::vector<PScriptEngineClientData>::iterator i = clients.begin(); i != clients.end(); ++i)
		{
			BaseLib::PArray parameters(new BaseLib::Array{deviceInfo});
			std::shared_ptr<BaseLib::IQueueEntry> queueEntry = std::make_shared<QueueEntry>(*i, "broadcastDeleteDevices", parameters);
			enqueue(1, queueEntry);
		}
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void ScriptEngineServer::broadcastUpdateDevice(uint64_t id, int32_t channel, int32_t hint)
{
	try
	{
		if(_shuttingDown) return;
		std::vector<PScriptEngineClientData> clients;
		{
			std::lock_guard<std::mutex> stateGuard(_stateMutex);
			for(std::map<int32_t, PScriptEngineClientData>::iterator i = _clients.begin(); i != _clients.end(); ++i)
			{
				if(i->second->closed) continue;
				clients.push_back(i->second);
			}
		}

		for(std::vector<PScriptEngineClientData>::iterator i = clients.begin(); i != clients.end(); ++i)
		{
			BaseLib::PArray parameters(new BaseLib::Array{BaseLib::PVariable(new BaseLib::Variable(id)), BaseLib::PVariable(new BaseLib::Variable(channel)), BaseLib::PVariable(new BaseLib::Variable(hint))});
			std::shared_ptr<BaseLib::IQueueEntry> queueEntry = std::make_shared<QueueEntry>(*i, "broadcastUpdateDevice", parameters);
			enqueue(1, queueEntry);
		}
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void ScriptEngineServer::closeClientConnection(PScriptEngineClientData client)
{
	try
	{
		if(!client) return;
		GD::bl->fileDescriptorManager.shutdown(client->fileDescriptor);
		client->closed = true;
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void ScriptEngineServer::processQueueEntry(int32_t index, std::shared_ptr<BaseLib::IQueueEntry>& entry)
{
	try
	{
		std::shared_ptr<QueueEntry> queueEntry;
		queueEntry = std::dynamic_pointer_cast<QueueEntry>(entry);
		if(!queueEntry || queueEntry->clientData->closed) return;

		if(index == 0 && queueEntry->type == QueueEntry::QueueEntryType::defaultType)
		{
			if(queueEntry->isRequest)
			{
				std::string methodName;
				BaseLib::PArray parameters = _rpcDecoder->decodeRequest(queueEntry->packet, methodName);

				if(parameters->size() != 3)
				{
					_out.printError("Error: Wrong parameter count while calling method " + methodName);
					return;
				}

				std::map<std::string, std::function<BaseLib::PVariable(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)>>::iterator localMethodIterator = _localRpcMethods.find(methodName);
				if(localMethodIterator != _localRpcMethods.end())
				{
					if(GD::bl->debugLevel >= 5)
					{
						_out.printDebug("Debug: Client number " + std::to_string(queueEntry->clientData->id) + " is calling RPC method: " + methodName);
						if(GD::bl->debugLevel >= 5)
						{
							for(BaseLib::Array::iterator i = parameters->at(2)->arrayValue->begin(); i != parameters->at(2)->arrayValue->end(); ++i)
							{
								(*i)->print(true, false);
							}
						}
					}
					BaseLib::PVariable result = localMethodIterator->second(queueEntry->clientData, parameters->at(0)->integerValue, parameters->at(2)->arrayValue);
					if(GD::bl->debugLevel >= 5)
					{
						_out.printDebug("Response: ");
						result->print(true, false);
					}

					sendResponse(queueEntry->clientData, parameters->at(0), parameters->at(1), result);
					return;
				}

				std::map<std::string, std::shared_ptr<BaseLib::Rpc::RpcMethod>>::iterator methodIterator = _rpcMethods.find(methodName);
				if(methodIterator == _rpcMethods.end())
				{
					BaseLib::PVariable result = GD::ipcServer->callRpcMethod(methodName, parameters->at(2)->arrayValue);
					sendResponse(queueEntry->clientData, parameters->at(0), parameters->at(1), result);
					return;
				}

				if(GD::bl->debugLevel >= 5)
				{
					_out.printInfo("Debug: Client number " + std::to_string(queueEntry->clientData->id) + " is calling RPC method: " + methodName + " Parameters:");
					for(std::vector<BaseLib::PVariable>::iterator i = parameters->at(2)->arrayValue->begin(); i != parameters->at(2)->arrayValue->end(); ++i)
					{
						(*i)->print(true, false);
					}
				}
				BaseLib::PVariable result = _rpcMethods.at(methodName)->invoke(_dummyClientInfo, parameters->at(2)->arrayValue);
				if(GD::bl->debugLevel >= 5)
				{
					_out.printDebug("Response: ");
					result->print(true, false);
				}

				sendResponse(queueEntry->clientData, parameters->at(0), parameters->at(1), result);
			}
			else
			{
				BaseLib::PVariable response = _rpcDecoder->decodeResponse(queueEntry->packet);
				int32_t packetId = response->arrayValue->at(0)->integerValue;

				{
					std::lock_guard<std::mutex> responseGuard(queueEntry->clientData->rpcResponsesMutex);
					auto responseIterator = queueEntry->clientData->rpcResponses.find(packetId);
					if(responseIterator != queueEntry->clientData->rpcResponses.end())
					{
						PScriptEngineResponse element = responseIterator->second;
						if(element)
						{
							element->response = response;
							element->packetId = packetId;
							element->finished = true;
						}
					}
				}
				queueEntry->clientData->requestConditionVariable.notify_all();
			}
		}
		else if(index == 1 && queueEntry->type == QueueEntry::QueueEntryType::broadcast) //Second queue for sending packets. Response is processed by first queue
		{
			BaseLib::PVariable response = sendRequest(queueEntry->clientData, queueEntry->methodName, queueEntry->parameters);
			if(response->errorStruct)
			{
				_out.printError("Error calling \"" + queueEntry->methodName + "\" on client " + std::to_string(queueEntry->clientData->id) + ": " + response->structValue->at("faultString")->stringValue);
			}
		}
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

BaseLib::PVariable ScriptEngineServer::send(PScriptEngineClientData& clientData, std::vector<char>& data)
{
	try
	{
		int32_t totallySentBytes = 0;
		std::lock_guard<std::mutex> sendGuard(clientData->sendMutex);
		while (totallySentBytes < (signed)data.size())
		{
			int32_t sentBytes = ::send(clientData->fileDescriptor->descriptor, &data.at(0) + totallySentBytes, data.size() - totallySentBytes, MSG_NOSIGNAL);
			if(sentBytes <= 0)
			{
				if(errno == EAGAIN) continue;
				GD::out.printError("Could not send data to client: " + std::to_string(clientData->fileDescriptor->descriptor));
				return BaseLib::Variable::createError(-32500, "Unknown application error.");
			}
			totallySentBytes += sentBytes;
		}
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return BaseLib::PVariable(new BaseLib::Variable());
}

BaseLib::PVariable ScriptEngineServer::sendRequest(PScriptEngineClientData& clientData, std::string methodName, BaseLib::PArray& parameters)
{
	try
	{
		int32_t packetId;
		{
			std::lock_guard<std::mutex> packetIdGuard(_packetIdMutex);
			packetId = _currentPacketId++;
		}
		BaseLib::PArray array(new BaseLib::Array{ BaseLib::PVariable(new BaseLib::Variable(packetId)), BaseLib::PVariable(new BaseLib::Variable(parameters)) });
		std::vector<char> data;
		_rpcEncoder->encodeRequest(methodName, array, data);

		PScriptEngineResponse response;
		{
			std::lock_guard<std::mutex> responseGuard(clientData->rpcResponsesMutex);
			auto result = clientData->rpcResponses.emplace(packetId, std::make_shared<ScriptEngineResponse>());
			if(result.second) response = result.first->second;
		}
		if(!response)
		{
			_out.printError("Critical: Could not insert response struct into map.");
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}

#ifdef DEBUGSESOCKET
		socketOutput(packetId, clientData, true, true, data);
#endif
		BaseLib::PVariable result = send(clientData, data);
		if(result->errorStruct)
		{
			std::lock_guard<std::mutex> responseGuard(clientData->rpcResponsesMutex);
			clientData->rpcResponses.erase(packetId);
			return result;
		}

		int32_t i = 0;
		std::unique_lock<std::mutex> waitLock(clientData->waitMutex);
		while(!clientData->requestConditionVariable.wait_for(waitLock, std::chrono::milliseconds(1000), [&]{
			return response->finished || clientData->closed || _stopServer;
		}))
		{
			i++;
			if(i == 60)
			{
				_out.printError("Error: Script engine client with process ID " + std::to_string(clientData->pid) + " is not responding... Killing it.");
				kill(clientData->pid, 9);
				processKilled(clientData->pid, -1, 9, false); //Needs to be called manually
				break;
			}
		}

		if(!response->finished || response->response->arrayValue->size() != 2 || response->packetId != packetId)
		{
			_out.printError("Error: No or invalid response received to RPC request. Method: " + methodName);
			 result = BaseLib::Variable::createError(-1, "No response received.");
		}
		else result = response->response->arrayValue->at(1);

		{
			std::lock_guard<std::mutex> responseGuard(clientData->rpcResponsesMutex);
			clientData->rpcResponses.erase(packetId);
		}

		return result;
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return BaseLib::Variable::createError(-32500, "Unknown application error.");
}

void ScriptEngineServer::sendResponse(PScriptEngineClientData& clientData, BaseLib::PVariable& scriptId, BaseLib::PVariable& packetId, BaseLib::PVariable& variable)
{
	try
	{
		BaseLib::PVariable array(new BaseLib::Variable(BaseLib::PArray(new BaseLib::Array{ scriptId, packetId, variable })));
		std::vector<char> data;
		_rpcEncoder->encodeResponse(array, data);
#ifdef DEBUGSESOCKET
		socketOutput(packetId->integerValue, clientData, false, false, data);
#endif
		send(clientData, data);
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void ScriptEngineServer::mainThread()
{
	try
	{
		int32_t result = 0;
		std::shared_ptr<BaseLib::FileDescriptor> clientFileDescriptor;
		while(!_stopServer)
		{
			if(!_serverFileDescriptor || _serverFileDescriptor->descriptor == -1)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				getFileDescriptor();
				continue;
			}

			timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 100000;
			fd_set readFileDescriptor;
			int32_t maxfd = 0;
			FD_ZERO(&readFileDescriptor);
			{
				auto fileDescriptorGuard = GD::bl->fileDescriptorManager.getLock();
				fileDescriptorGuard.lock();
				maxfd = _serverFileDescriptor->descriptor;
				FD_SET(_serverFileDescriptor->descriptor, &readFileDescriptor);

				{
					std::lock_guard<std::mutex> stateGuard(_stateMutex);
					for(std::map<int32_t, PScriptEngineClientData>::iterator i = _clients.begin(); i != _clients.end(); ++i)
					{
						if(i->second->closed) continue;
						if(i->second->fileDescriptor->descriptor == -1)
						{
							i->second->closed = true;
							continue;
						}
						FD_SET(i->second->fileDescriptor->descriptor, &readFileDescriptor);
						if(i->second->fileDescriptor->descriptor > maxfd) maxfd = i->second->fileDescriptor->descriptor;
					}
				}
			}

			result = select(maxfd + 1, &readFileDescriptor, NULL, NULL, &timeout);
			if(result == 0)
			{
				if(GD::bl->hf.getTime() - _lastGargabeCollection > 60000 || _clients.size() > GD::bl->settings.scriptEngineServerMaxConnections() * 100 / 112) collectGarbage();
				continue;
			}
			else if(result == -1)
			{
				if(errno == EINTR) continue;
				_out.printError("Error: select returned -1: " + std::string(strerror(errno)));
				continue;
			}

			if (FD_ISSET(_serverFileDescriptor->descriptor, &readFileDescriptor) && !_shuttingDown)
			{
				sockaddr_un clientAddress;
				socklen_t addressSize = sizeof(addressSize);
				std::shared_ptr<BaseLib::FileDescriptor> clientFileDescriptor = GD::bl->fileDescriptorManager.add(accept(_serverFileDescriptor->descriptor, (struct sockaddr *) &clientAddress, &addressSize));
				if(!clientFileDescriptor || clientFileDescriptor->descriptor == -1) continue;
				_out.printInfo("Info: Connection accepted. Client number: " + std::to_string(clientFileDescriptor->id));

				if(_clients.size() > GD::bl->settings.scriptEngineServerMaxConnections())
				{
					collectGarbage();
					if(_clients.size() > GD::bl->settings.scriptEngineServerMaxConnections())
					{
						_out.printError("Error: There are too many clients connected to me. Closing connection. You can increase the number of allowed connections in main.conf.");
						GD::bl->fileDescriptorManager.close(clientFileDescriptor);
						continue;
					}
				}

				std::lock_guard<std::mutex> stateGuard(_stateMutex);
				if(_shuttingDown)
				{
					GD::bl->fileDescriptorManager.close(clientFileDescriptor);
					continue;
				}
				PScriptEngineClientData clientData = PScriptEngineClientData(new ScriptEngineClientData(clientFileDescriptor));
				clientData->id = _currentClientId++;
				_clients[clientData->id] = clientData;
				continue;
			}

			PScriptEngineClientData clientData;
			{
				std::lock_guard<std::mutex> stateGuard(_stateMutex);
				for(std::map<int32_t, PScriptEngineClientData>::iterator i = _clients.begin(); i != _clients.end(); ++i)
				{
					if(i->second->fileDescriptor->descriptor == -1)
					{
						i->second->closed = true;
						continue;
					}
					if(FD_ISSET(i->second->fileDescriptor->descriptor, &readFileDescriptor))
					{
						clientData = i->second;
						break;
					}
				}
			}

			if(clientData) readClient(clientData);
		}
		GD::bl->fileDescriptorManager.close(_serverFileDescriptor);
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

PScriptEngineProcess ScriptEngineServer::getFreeProcess(bool nodeProcess, uint32_t maxThreadCount)
{
	try
	{
		std::lock_guard<std::mutex> processGuard(_newProcessMutex);

		if(nodeProcess && GD::bl->settings.maxNodeThreadsPerProcess() != -1 && maxThreadCount + 1 > (unsigned)GD::bl->settings.maxNodeThreadsPerProcess())
		{
			GD::out.printError("Error: Could not get script process for node, because maximum number of threads in node is greater than the number of threads allowed per process.");
			return std::shared_ptr<ScriptEngineProcess>();
		}

		{
			std::lock_guard<std::mutex> processGuard(_processMutex);
			for(std::map<pid_t, std::shared_ptr<ScriptEngineProcess>>::iterator i = _processes.begin(); i != _processes.end(); ++i)
			{
				if(nodeProcess)
				{
					if(GD::bl->settings.maxNodeThreadsPerProcess() == -1 || i->second->nodeThreadCount() + maxThreadCount + 1 <= (unsigned)GD::bl->settings.maxNodeThreadsPerProcess())
					{
						return i->second;
					}
				}
				else if(i->second->scriptCount() < GD::bl->threadManager.getMaxThreadCount() / GD::bl->settings.scriptEngineMaxThreadsPerScript() && (GD::bl->settings.scriptEngineMaxScriptsPerProcess() == -1 || i->second->scriptCount() < (unsigned)GD::bl->settings.scriptEngineMaxScriptsPerProcess()))
				{
					return i->second;
				}
			}
		}
		_out.printInfo("Info: Spawning new script engine process.");
		std::shared_ptr<ScriptEngineProcess> process(new ScriptEngineProcess(nodeProcess));
		std::vector<std::string> arguments{ "-c", GD::configPath, "-rse" };
#ifdef MANUAL_CLIENT_START
		process->setPid(1);
#else
		process->setPid(GD::bl->hf.system(GD::executablePath + "/" + GD::executableFile, arguments));
#endif
		if(process->getPid() != -1)
		{
			{
				std::lock_guard<std::mutex> processGuard(_processMutex);
				_processes[process->getPid()] = process;
			}

			std::mutex requestMutex;
			std::unique_lock<std::mutex> requestLock(requestMutex);
			process->requestConditionVariable.wait_for(requestLock, std::chrono::milliseconds(60000), [&]{ return (bool)(process->getClientData()); });

			if(!process->getClientData())
			{
				std::lock_guard<std::mutex> processGuard(_processMutex);
				_processes.erase(process->getPid());
				_out.printError("Error: Could not start new script engine process.");
				return std::shared_ptr<ScriptEngineProcess>();
			}
			process->setUnregisterNode(std::function<void(std::string&)>(std::bind(&ScriptEngineServer::unregisterNode, this, std::placeholders::_1)));
			_out.printInfo("Info: Script engine process successfully spawned. Process id is " + std::to_string(process->getPid()) + ". Client id is: " + std::to_string(process->getClientData()->id) + ".");
			return process;
		}
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return std::shared_ptr<ScriptEngineProcess>();
}

void ScriptEngineServer::readClient(PScriptEngineClientData& clientData)
{
	try
	{
		int32_t processedBytes = 0;
		int32_t bytesRead = 0;
		bytesRead = read(clientData->fileDescriptor->descriptor, clientData->buffer.data(), clientData->buffer.size());
		if(bytesRead <= 0) //read returns 0, when connection is disrupted.
		{
			_out.printInfo("Info: Connection to script server's client number " + std::to_string(clientData->fileDescriptor->id) + " closed.");
			closeClientConnection(clientData);
			return;
		}

		if(bytesRead > (signed)clientData->buffer.size()) bytesRead = clientData->buffer.size();

		try
		{
			processedBytes = 0;
			while(processedBytes < bytesRead)
			{
				processedBytes += clientData->binaryRpc->process(&(clientData->buffer[processedBytes]), bytesRead - processedBytes);
				if(clientData->binaryRpc->isFinished())
				{
#ifdef DEBUGSESOCKET
					if(clientData->binaryRpc->getType() == BaseLib::Rpc::BinaryRpc::Type::request)
					{
						std::string methodName;
						BaseLib::PArray request = _rpcDecoder->decodeRequest(clientData->binaryRpc->getData(), methodName);
						socketOutput(request->at(1)->integerValue, clientData, false, true, clientData->binaryRpc->getData());
					}
					else
					{
						BaseLib::PVariable response = _rpcDecoder->decodeResponse(clientData->binaryRpc->getData());
						socketOutput(response->arrayValue->at(0)->integerValue, clientData, true, false, clientData->binaryRpc->getData());
					}
#endif
					std::shared_ptr<BaseLib::IQueueEntry> queueEntry(new QueueEntry(clientData, clientData->binaryRpc->getData(), clientData->binaryRpc->getType() == BaseLib::Rpc::BinaryRpc::Type::request));
					enqueue(0, queueEntry);
					clientData->binaryRpc->reset();
				}
			}
		}
		catch(BaseLib::Rpc::BinaryRpcException& ex)
		{
			_out.printError("Error processing packet: " + ex.what());
			clientData->binaryRpc->reset();
		}
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

bool ScriptEngineServer::getFileDescriptor(bool deleteOldSocket)
{
	try
	{
		if(!BaseLib::Io::directoryExists(GD::bl->settings.socketPath().c_str()))
		{
			if(errno == ENOENT) _out.printCritical("Critical: Directory " + GD::bl->settings.socketPath() + " does not exist. Please create it before starting Homegear otherwise the script engine won't work.");
			else _out.printCritical("Critical: Error reading information of directory " + GD::bl->settings.socketPath() + ". The script engine won't work: " + strerror(errno));
			_stopServer = true;
			return false;
		}
		bool isDirectory = false;
		int32_t result = BaseLib::Io::isDirectory(GD::bl->settings.socketPath(), isDirectory);
		if(result != 0 || !isDirectory)
		{
			_out.printCritical("Critical: Directory " + GD::bl->settings.socketPath() + " does not exist. Please create it before starting Homegear otherwise the script engine interface won't work.");
			_stopServer = true;
			return false;
		}
		if(deleteOldSocket)
		{
			if(unlink(_socketPath.c_str()) == -1 && errno != ENOENT)
			{
				_out.printCritical("Critical: Couldn't delete existing socket: " + _socketPath + ". Please delete it manually. The script engine won't work. Error: " + strerror(errno));
				return false;
			}
		}
		else if(BaseLib::Io::fileExists(_socketPath)) return false;

		_serverFileDescriptor = GD::bl->fileDescriptorManager.add(socket(AF_LOCAL, SOCK_STREAM | SOCK_NONBLOCK, 0));
		if(_serverFileDescriptor->descriptor == -1)
		{
			_out.printCritical("Critical: Couldn't create socket: " + _socketPath + ". The script engine won't work. Error: " + strerror(errno));
			return false;
		}
		int32_t reuseAddress = 1;
		if(setsockopt(_serverFileDescriptor->descriptor, SOL_SOCKET, SO_REUSEADDR, (void*)&reuseAddress, sizeof(int32_t)) == -1)
		{
			GD::bl->fileDescriptorManager.close(_serverFileDescriptor);
			_out.printCritical("Critical: Couldn't set socket options: " + _socketPath + ". The script engine won't work correctly. Error: " + strerror(errno));
			return false;
		}
		sockaddr_un serverAddress;
		serverAddress.sun_family = AF_LOCAL;
		//104 is the size on BSD systems - slightly smaller than in Linux
		if(_socketPath.length() > 104)
		{
			//Check for buffer overflow
			_out.printCritical("Critical: Socket path is too long.");
			return false;
		}
		strncpy(serverAddress.sun_path, _socketPath.c_str(), 104);
		serverAddress.sun_path[103] = 0; //Just to make sure the string is null terminated.
		bool bound = (bind(_serverFileDescriptor->descriptor, (sockaddr*)&serverAddress, strlen(serverAddress.sun_path) + 1 + sizeof(serverAddress.sun_family)) != -1);
		if(_serverFileDescriptor->descriptor == -1 || !bound || listen(_serverFileDescriptor->descriptor, _backlog) == -1)
		{
			GD::bl->fileDescriptorManager.close(_serverFileDescriptor);
			_out.printCritical("Critical: Script engine server could not start listening. Error: " + std::string(strerror(errno)));
		}
		if(chmod(_socketPath.c_str(), S_IRWXU | S_IRWXG) == -1)
		{
			_out.printError("Error: chmod failed on unix socket \"" + _socketPath + "\".");
		}
		return true;
    }
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    GD::bl->fileDescriptorManager.close(_serverFileDescriptor);
    return false;
}

bool ScriptEngineServer::checkSessionId(const std::string& sessionId)
{
	try
	{
		if(_shuttingDown) return false;
		PScriptEngineClientData client;

		{
			std::lock_guard<std::mutex> stateGuard(_stateMutex);
			for(std::map<int32_t, PScriptEngineClientData>::iterator i = _clients.begin(); i != _clients.end(); ++i)
			{
				if(i->second->closed) continue;
				client = i->second;
				break;
			}
		}
		if(!client)
		{
			PScriptEngineProcess process = getFreeProcess(false, 0);
			if(!process)
			{
				GD::out.printError("Error: Could not check session ID. Could not get free process.");
				return false;
			}
			client = process->getClientData();
		}
		BaseLib::PArray parameters = std::make_shared<BaseLib::Array>();
		parameters->push_back(std::make_shared<BaseLib::Variable>(sessionId));
		BaseLib::PVariable result = sendRequest(client, "checkSessionId", parameters);
		if(result->errorStruct)
		{
			GD::out.printError("Error: checkSessionId returned: " + result->structValue->at("faultString")->stringValue);
			return false;
		}
		return result->booleanValue;
	}
	catch(const std::exception& ex)
	{
		GD::bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return false;
}

void ScriptEngineServer::invokeScriptFinished(PScriptEngineProcess process, int32_t id, int32_t exitCode)
{
	try
	{
		if(id == -1) process->invokeScriptFinished(exitCode);
		else
		{
			process->invokeScriptFinished(id, exitCode);
			process->unregisterScript(id);
		}
	}
	catch(const std::exception& ex)
	{
		GD::bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
}

void ScriptEngineServer::invokeScriptFinishedEarly(PScriptInfo scriptInfo, int32_t exitCode)
{
	try
	{
		if(scriptInfo->scriptFinishedCallback) scriptInfo->scriptFinishedCallback(scriptInfo, exitCode);
	}
	catch(const std::exception& ex)
	{
		GD::bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
}

void ScriptEngineServer::executeScript(PScriptInfo& scriptInfo, bool wait)
{
	try
	{
		if(_shuttingDown || GD::bl->shuttingDown) return;
		PScriptEngineProcess process = getFreeProcess(scriptInfo->getType() == BaseLib::ScriptEngine::ScriptInfo::ScriptType::statefulNode, scriptInfo->maxThreadCount);
		if(!process)
		{
			_out.printError("Error: Could not get free process. Not executing script.");
			if(scriptInfo->returnOutput) scriptInfo->output.append("Error: Could not get free process. Not executing script.\n");
			scriptInfo->exitCode = -1;

			{
				std::lock_guard<std::mutex> scriptFinishedGuard(_scriptFinishedThreadMutex);
				GD::bl->threadManager.start(_scriptFinishedThread, true, &ScriptEngineServer::invokeScriptFinishedEarly, this, scriptInfo, -1);
			}

			return;
		}

		{
			std::lock_guard<std::mutex> scriptIdGuard(_currentScriptIdMutex);
			while(scriptInfo->id == 0) scriptInfo->id = _currentScriptId++;
		}

		if(GD::bl->debugLevel >= 5 || (scriptInfo->getType() != BaseLib::ScriptEngine::ScriptInfo::ScriptType::statefulNode && scriptInfo->getType() != BaseLib::ScriptEngine::ScriptInfo::ScriptType::simpleNode))
		{
			_out.printInfo("Info: Starting script \"" + scriptInfo->fullPath + "\" with id " + std::to_string(scriptInfo->id) + ".");
		}
		process->registerScript(scriptInfo->id, scriptInfo);

		PScriptEngineClientData clientData = process->getClientData();
		PScriptFinishedInfo scriptFinishedInfo;
		std::unique_lock<std::mutex> scriptFinishedLock;
		if(wait)
		{
			scriptFinishedInfo = process->getScriptFinishedInfo(scriptInfo->id);
			scriptFinishedLock = std::move(std::unique_lock<std::mutex>(scriptFinishedInfo->mutex));
		}

		BaseLib::PArray parameters;
		ScriptInfo::ScriptType scriptType = scriptInfo->getType();
		if(scriptType == ScriptInfo::ScriptType::cli)
		{
			parameters = std::move(BaseLib::PArray(new BaseLib::Array{
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->id)),
				BaseLib::PVariable(new BaseLib::Variable((int32_t)scriptInfo->getType())),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->fullPath)),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->relativePath)),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->script)),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->arguments)),
				BaseLib::PVariable(new BaseLib::Variable((bool)scriptInfo->scriptOutputCallback || scriptInfo->returnOutput))}));
		}
		else if(scriptType == ScriptInfo::ScriptType::web)
		{
			parameters = std::move(BaseLib::PArray(new BaseLib::Array{
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->id)),
				BaseLib::PVariable(new BaseLib::Variable((int32_t)scriptInfo->getType())),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->fullPath)),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->relativePath)),
				scriptInfo->http.serialize(),
				scriptInfo->serverInfo->serialize()}));
		}
		else if(scriptType == ScriptInfo::ScriptType::device)
		{
			parameters = std::move(BaseLib::PArray(new BaseLib::Array{
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->id)),
				BaseLib::PVariable(new BaseLib::Variable((int32_t)scriptInfo->getType())),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->fullPath)),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->relativePath)),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->script)),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->arguments)),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->peerId))}));
		}
		else if(scriptType == ScriptInfo::ScriptType::simpleNode)
		{
			parameters = std::move(BaseLib::PArray(new BaseLib::Array{
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->id)),
				BaseLib::PVariable(new BaseLib::Variable((int32_t)scriptInfo->getType())),
				scriptInfo->nodeInfo,
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->fullPath)),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->relativePath)),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->inputPort)),
				scriptInfo->message}));
		}
		else if(scriptType == ScriptInfo::ScriptType::statefulNode)
		{
			parameters = std::move(BaseLib::PArray(new BaseLib::Array{
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->id)),
				BaseLib::PVariable(new BaseLib::Variable((int32_t)scriptInfo->getType())),
				scriptInfo->nodeInfo,
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->fullPath)),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->relativePath)),
				BaseLib::PVariable(new BaseLib::Variable(scriptInfo->maxThreadCount))}));

			{
				std::lock_guard<std::mutex> nodeClientIdMapGuard(_nodeClientIdMapMutex);
				_nodeClientIdMap.erase(scriptInfo->nodeInfo->structValue->at("id")->stringValue);
				_nodeClientIdMap.emplace(scriptInfo->nodeInfo->structValue->at("id")->stringValue, clientData->id);
			}
		}

		BaseLib::PVariable result = sendRequest(clientData, "executeScript", parameters);
		if(result->errorStruct)
		{
			_out.printError("Error: Could not execute script: " + result->structValue->at("faultString")->stringValue);
			if(scriptInfo->returnOutput) scriptInfo->output.append("Error: Could not execute script: " + result->structValue->at("faultString")->stringValue + '\n');

			{
				std::lock_guard<std::mutex> scriptFinishedGuard(_scriptFinishedThreadMutex);
				GD::bl->threadManager.start(_scriptFinishedThread, true, &ScriptEngineServer::invokeScriptFinished, this, process, scriptInfo->id, result->structValue->at("faultCode")->integerValue);
			}

			if(scriptType == ScriptInfo::ScriptType::statefulNode)
			{
				std::lock_guard<std::mutex> nodeClientIdMapGuard(_nodeClientIdMapMutex);
				_nodeClientIdMap.erase(scriptInfo->nodeInfo->structValue->at("id")->stringValue);
			}

			return;
		}
		scriptInfo->started = true;
		if(wait)
		{
			while(!scriptFinishedInfo->conditionVariable.wait_for(scriptFinishedLock, std::chrono::milliseconds(10000), [&]{ return scriptFinishedInfo->finished || clientData->closed || _stopServer; }));
		}
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

BaseLib::PVariable ScriptEngineServer::getAllScripts()
{
	try
	{
		BaseLib::PVariable array(new BaseLib::Variable(BaseLib::VariableType::tArray));

		std::vector<std::string> scripts = GD::bl->io.getFiles(GD::bl->settings.scriptPath(), true);
		for(std::vector<std::string>::iterator i = scripts.begin(); i != scripts.end(); ++i)
		{
			array->arrayValue->push_back(BaseLib::PVariable(new BaseLib::Variable(*i)));
		}

		return array;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return BaseLib::Variable::createError(-32500, "Unknown application error.");
}

void ScriptEngineServer::unregisterNode(std::string& nodeId)
{
	try
	{
		{
			std::lock_guard<std::mutex> nodeClientIdMapGuard(_nodeClientIdMapMutex);
			_nodeClientIdMap.erase(nodeId);
		}
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
}

// {{{ RPC methods
	BaseLib::PVariable ScriptEngineServer::registerScriptEngineClient(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
	{
		try
		{
			pid_t pid = parameters->at(0)->integerValue;
#ifdef MANUAL_CLIENT_START
			pid = 1;
#endif
			std::lock_guard<std::mutex> processGuard(_processMutex);
			std::map<pid_t, std::shared_ptr<ScriptEngineProcess>>::iterator processIterator = _processes.find(pid);
			if(processIterator == _processes.end())
			{
				_out.printError("Error: Cannot register client. No process with pid " + std::to_string(pid) + " found.");
				return BaseLib::Variable::createError(-1, "No matching process found.");
			}
			if(processIterator->second->getClientData())
			{
				_out.printError("Error: Cannot register client, because it already is registered.");
				return BaseLib::PVariable(new BaseLib::Variable());
			}
			clientData->pid = pid;
			processIterator->second->setClientData(clientData);
			processIterator->second->requestConditionVariable.notify_all();
			return BaseLib::PVariable(new BaseLib::Variable());
		}
		catch(const std::exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(BaseLib::Exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(...)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
		}
		return BaseLib::Variable::createError(-32500, "Unknown application error.");
	}

	BaseLib::PVariable ScriptEngineServer::scriptFinished(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
	{
		try
		{
			if(parameters->size() < 1) return BaseLib::Variable::createError(-1, "Wrong parameter count.");

			int32_t exitCode = parameters->at(0)->integerValue;
			std::lock_guard<std::mutex> processGuard(_processMutex);
			std::map<pid_t, std::shared_ptr<ScriptEngineProcess>>::iterator processIterator = _processes.find(clientData->pid);
			if(processIterator == _processes.end()) return BaseLib::Variable::createError(-1, "No matching process found.");
			processIterator->second->invokeScriptFinished(scriptId, exitCode);
			processIterator->second->unregisterScript(scriptId);
			return BaseLib::PVariable(new BaseLib::Variable());
		}
		catch(const std::exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(BaseLib::Exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(...)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
		}
		return BaseLib::Variable::createError(-32500, "Unknown application error.");
	}

	BaseLib::PVariable ScriptEngineServer::scriptHeaders(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
	{
		try
		{
			if(parameters->size() < 1) return BaseLib::Variable::createError(-1, "Wrong parameter count.");

			std::lock_guard<std::mutex> processGuard(_processMutex);
			std::map<pid_t, std::shared_ptr<ScriptEngineProcess>>::iterator processIterator = _processes.find(clientData->pid);
			if(processIterator == _processes.end()) return BaseLib::Variable::createError(-1, "No matching process found.");
			processIterator->second->invokeScriptHeaders(scriptId, parameters->at(0));
			return BaseLib::PVariable(new BaseLib::Variable());
		}
		catch(const std::exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(BaseLib::Exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(...)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
		}
		return BaseLib::Variable::createError(-32500, "Unknown application error.");
	}

	BaseLib::PVariable ScriptEngineServer::scriptOutput(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
	{
		try
		{
			if(parameters->size() < 1) return BaseLib::Variable::createError(-1, "Wrong parameter count.");

			std::lock_guard<std::mutex> processGuard(_processMutex);
			std::map<pid_t, std::shared_ptr<ScriptEngineProcess>>::iterator processIterator = _processes.find(clientData->pid);
			if(processIterator == _processes.end()) return BaseLib::Variable::createError(-1, "No matching process found.");
			processIterator->second->invokeScriptOutput(scriptId, parameters->at(0)->stringValue);
			return BaseLib::PVariable(new BaseLib::Variable());
		}
		catch(const std::exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(BaseLib::Exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(...)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
		}
		return BaseLib::Variable::createError(-32500, "Unknown application error.");
	}

	BaseLib::PVariable ScriptEngineServer::peerExists(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
	{
		try
		{
			if(parameters->size() != 1) return BaseLib::Variable::createError(-1, "Wrong parameter count.");
			if(parameters->at(0)->type != BaseLib::VariableType::tInteger) return BaseLib::Variable::createError(-1, "Parameter is not of type integer.");

			return BaseLib::PVariable(new BaseLib::Variable(GD::familyController->peerExists(parameters->at(0)->integerValue)));
		}
		catch(const std::exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(BaseLib::Exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(...)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
		}
		return BaseLib::Variable::createError(-32500, "Unknown application error.");
	}

	BaseLib::PVariable ScriptEngineServer::listRpcClients(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
	{
		try
		{
			if(parameters->size() != 0) return BaseLib::Variable::createError(-1, "Method doesn't expect any parameters.");

			BaseLib::PVariable result(new BaseLib::Variable(BaseLib::VariableType::tArray));
			for(std::map<int32_t, Rpc::Server>::iterator i = GD::rpcServers.begin(); i != GD::rpcServers.end(); ++i)
			{
				const std::vector<BaseLib::PRpcClientInfo> clients = i->second.getClientInfo();
				for(std::vector<BaseLib::PRpcClientInfo>::const_iterator j = clients.begin(); j != clients.end(); ++j)
				{
					BaseLib::PVariable element(new BaseLib::Variable(BaseLib::VariableType::tStruct));

					element->structValue->insert(BaseLib::StructElement("CLIENT_ID", BaseLib::PVariable(new BaseLib::Variable((*j)->id))));
					element->structValue->insert(BaseLib::StructElement("IP_ADDRESS", BaseLib::PVariable(new BaseLib::Variable((*j)->address))));
					element->structValue->insert(BaseLib::StructElement("INIT_URL", BaseLib::PVariable(new BaseLib::Variable((*j)->initUrl))));
					element->structValue->insert(BaseLib::StructElement("INIT_INTERFACE_ID", BaseLib::PVariable(new BaseLib::Variable((*j)->initInterfaceId))));
					element->structValue->insert(BaseLib::StructElement("XML_RPC", BaseLib::PVariable(new BaseLib::Variable((*j)->rpcType == BaseLib::RpcType::xml))));
					element->structValue->insert(BaseLib::StructElement("BINARY_RPC", BaseLib::PVariable(new BaseLib::Variable((*j)->rpcType == BaseLib::RpcType::binary))));
					element->structValue->insert(BaseLib::StructElement("JSON_RPC", BaseLib::PVariable(new BaseLib::Variable((*j)->rpcType == BaseLib::RpcType::json))));
					element->structValue->insert(BaseLib::StructElement("WEBSOCKET", BaseLib::PVariable(new BaseLib::Variable((*j)->rpcType == BaseLib::RpcType::websocket))));
					element->structValue->insert(BaseLib::StructElement("INIT_KEEP_ALIVE", BaseLib::PVariable(new BaseLib::Variable((*j)->initKeepAlive))));
					element->structValue->insert(BaseLib::StructElement("INIT_BINARY_MODE", BaseLib::PVariable(new BaseLib::Variable((*j)->initBinaryMode))));
					element->structValue->insert(BaseLib::StructElement("INIT_NEW_FORMAT", BaseLib::PVariable(new BaseLib::Variable((*j)->initNewFormat))));
					element->structValue->insert(BaseLib::StructElement("INIT_SUBSCRIBE_PEERS", BaseLib::PVariable(new BaseLib::Variable((*j)->initSubscribePeers))));
					element->structValue->insert(BaseLib::StructElement("INIT_JSON_MODE", BaseLib::PVariable(new BaseLib::Variable((*j)->initJsonMode))));

					result->arrayValue->push_back(element);
				}
			}

			return result;
		}
		catch(const std::exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(BaseLib::Exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(...)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
		}
		return BaseLib::Variable::createError(-32500, "Unknown application error.");
	}

	BaseLib::PVariable ScriptEngineServer::raiseDeleteDevice(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
	{
		try
		{
			if(parameters->size() != 1 || parameters->at(0)->type != BaseLib::VariableType::tArray) return BaseLib::Variable::createError(-1, "Method expects device address array as parameter.");

			GD::familyController->onRPCDeleteDevices(parameters->at(0), BaseLib::PVariable(new BaseLib::Variable(BaseLib::PArray(new BaseLib::Array{ 0 }))));

			return BaseLib::PVariable(new BaseLib::Variable());
		}
		catch(const std::exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(BaseLib::Exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(...)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
		}
		return BaseLib::Variable::createError(-32500, "Unknown application error.");
	}

	BaseLib::PVariable ScriptEngineServer::getFamilySetting(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
	{
		try
		{
			if(parameters->size() != 2 || parameters->at(0)->type != BaseLib::VariableType::tInteger || parameters->at(1)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Method expects family ID and parameter name as parameters.");

			std::shared_ptr<BaseLib::Systems::DeviceFamily> family = GD::familyController->getFamily(parameters->at(0)->integerValue);
			if(!family) return BaseLib::Variable::createError(-2, "Device family not found.");

			BaseLib::Systems::FamilySettings::PFamilySetting setting = family->getFamilySetting(parameters->at(1)->stringValue);
			if(!setting) return BaseLib::Variable::createError(-3, "Setting not found.");

			if(!setting->stringValue.empty()) return BaseLib::PVariable(new BaseLib::Variable(setting->stringValue));
			else if(!setting->binaryValue.empty()) return BaseLib::PVariable(new BaseLib::Variable(setting->binaryValue));
			else return BaseLib::PVariable(new BaseLib::Variable(setting->integerValue));
		}
		catch(const std::exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(BaseLib::Exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(...)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
		}
		return BaseLib::Variable::createError(-32500, "Unknown application error.");
	}

	BaseLib::PVariable ScriptEngineServer::setFamilySetting(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
	{
		try
		{
			if(parameters->size() != 3 || parameters->at(0)->type != BaseLib::VariableType::tInteger || parameters->at(1)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Method expects family ID, parameter name and value as parameters.");

			std::shared_ptr<BaseLib::Systems::DeviceFamily> family = GD::familyController->getFamily(parameters->at(0)->integerValue);
			if(!family) return BaseLib::Variable::createError(-2, "Device family not found.");

			if(parameters->at(2)->type == BaseLib::VariableType::tString) family->setFamilySetting(parameters->at(1)->stringValue, parameters->at(2)->stringValue);
			else if(parameters->at(2)->type == BaseLib::VariableType::tInteger) family->setFamilySetting(parameters->at(1)->stringValue, parameters->at(2)->integerValue);
			else if(parameters->at(2)->type == BaseLib::VariableType::tBinary)
			{
				std::vector<char> data(&parameters->at(2)->binaryValue.at(0), &parameters->at(2)->binaryValue.at(0) + parameters->at(2)->binaryValue.size());
				family->setFamilySetting(parameters->at(1)->stringValue, data);
			}
			else return BaseLib::Variable::createError(-3, "Unsupported variable type.");

			return BaseLib::PVariable(new BaseLib::Variable());
		}
		catch(const std::exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(BaseLib::Exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(...)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
		}
		return BaseLib::Variable::createError(-32500, "Unknown application error.");
	}

	BaseLib::PVariable ScriptEngineServer::deleteFamilySetting(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
	{
		try
		{
			if(parameters->size() != 2 || parameters->at(0)->type != BaseLib::VariableType::tInteger || parameters->at(1)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Method expects family ID and parameter name as parameters.");

			std::shared_ptr<BaseLib::Systems::DeviceFamily> family = GD::familyController->getFamily(parameters->at(0)->integerValue);
			if(!family) return BaseLib::Variable::createError(-2, "Device family not found.");

			family->deleteFamilySettingFromDatabase(parameters->at(1)->stringValue);

			return BaseLib::PVariable(new BaseLib::Variable());
		}
		catch(const std::exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(BaseLib::Exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(...)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
		}
		return BaseLib::Variable::createError(-32500, "Unknown application error.");
	}

	// {{{ MQTT
		BaseLib::PVariable ScriptEngineServer::mqttPublish(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 2) return BaseLib::Variable::createError(-1, "Method expects exactly two parameters (topic and payload).");
				if(parameters->at(0)->type != BaseLib::VariableType::tString || parameters->at(1)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Parameters are not of type string.");
				if(parameters->at(0)->stringValue.empty()) return BaseLib::Variable::createError(-1, "Topic is empty.");
				if(parameters->at(1)->stringValue.empty()) return BaseLib::Variable::createError(-1, "Payload is empty.");

				GD::mqtt->queueMessage(parameters->at(0)->stringValue, parameters->at(1)->stringValue);
				return std::make_shared<BaseLib::Variable>();
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}
	// }}}

	// {{{ User methods
		BaseLib::PVariable ScriptEngineServer::auth(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 2) return BaseLib::Variable::createError(-1, "Method expects exactly two parameters.");
				if(parameters->at(0)->type != BaseLib::VariableType::tString || parameters->at(1)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Parameters are not of type string.");
				if(parameters->at(0)->stringValue.empty()) return BaseLib::Variable::createError(-1, "Parameter 1 is empty.");
				if(parameters->at(1)->stringValue.empty()) return BaseLib::Variable::createError(-1, "Parameter 2 is empty.");

				return BaseLib::PVariable(new BaseLib::Variable(User::verify(parameters->at(0)->stringValue, parameters->at(1)->stringValue)));
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}

		BaseLib::PVariable ScriptEngineServer::createUser(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 2) return BaseLib::Variable::createError(-1, "Method expects exactly two parameters.");
				if(parameters->at(0)->type != BaseLib::VariableType::tString || parameters->at(1)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Parameters are not of type string.");
				if(parameters->at(0)->stringValue.empty()) return BaseLib::Variable::createError(-1, "Parameter 1 is empty.");
				if(parameters->at(1)->stringValue.empty()) return BaseLib::Variable::createError(-1, "Parameter 2 is empty.");
				if(!BaseLib::HelperFunctions::isAlphaNumeric(parameters->at(0)->stringValue)) return BaseLib::Variable::createError(-1, "Parameter 1 is not alphanumeric.");

				return BaseLib::PVariable(new BaseLib::Variable(User::create(parameters->at(0)->stringValue, parameters->at(1)->stringValue)));
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}

		BaseLib::PVariable ScriptEngineServer::deleteUser(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 1) return BaseLib::Variable::createError(-1, "Method expects exactly one parameter.");
				if(parameters->at(0)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Parameter is not of type string.");
				if(parameters->at(0)->stringValue.empty()) return BaseLib::Variable::createError(-1, "Parameter is empty.");

				return BaseLib::PVariable(new BaseLib::Variable(User::remove(parameters->at(0)->stringValue)));
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}

		BaseLib::PVariable ScriptEngineServer::updateUser(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 2) return BaseLib::Variable::createError(-1, "Method expects exactly two parameters.");
				if(parameters->at(0)->type != BaseLib::VariableType::tString || parameters->at(1)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Parameters are not of type string.");
				if(parameters->at(0)->stringValue.empty()) return BaseLib::Variable::createError(-1, "Parameter 1 is empty.");
				if(parameters->at(1)->stringValue.empty()) return BaseLib::Variable::createError(-1, "Parameter 2 is empty.");

				return BaseLib::PVariable(new BaseLib::Variable(User::update(parameters->at(0)->stringValue, parameters->at(1)->stringValue)));
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}

		BaseLib::PVariable ScriptEngineServer::userExists(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 1) return BaseLib::Variable::createError(-1, "Method expects exactly one parameter.");
				if(parameters->at(0)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Parameter is not of type string.");
				if(parameters->at(0)->stringValue.empty()) return BaseLib::Variable::createError(-1, "Parameter is empty.");

				return BaseLib::PVariable(new BaseLib::Variable(User::exists(parameters->at(0)->stringValue)));
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}

		BaseLib::PVariable ScriptEngineServer::listUsers(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 0) return BaseLib::Variable::createError(-1, "Method doesn't expect any parameters.");

				std::map<uint64_t, std::string> users;
				User::getAll(users);

				BaseLib::PVariable result(new BaseLib::Variable(BaseLib::VariableType::tArray));
				result->arrayValue->reserve(users.size());
				for(std::map<uint64_t, std::string>::iterator i = users.begin(); i != users.end(); ++i)
				{
					result->arrayValue->push_back(BaseLib::PVariable(new BaseLib::Variable(i->second)));
				}

				return result;
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}
	// }}}

	// {{{ Module methods
		BaseLib::PVariable ScriptEngineServer::listModules(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 0) return BaseLib::Variable::createError(-1, "Method doesn't expect any parameters.");

				std::vector<std::shared_ptr<FamilyController::ModuleInfo>> moduleInfo = GD::familyController->getModuleInfo();

				BaseLib::PVariable result(new BaseLib::Variable(BaseLib::VariableType::tArray));
				result->arrayValue->reserve(moduleInfo.size());
				for(std::vector<std::shared_ptr<FamilyController::ModuleInfo>>::iterator i = moduleInfo.begin(); i != moduleInfo.end(); ++i)
				{
					BaseLib::PVariable element(new BaseLib::Variable(BaseLib::VariableType::tStruct));

					element->structValue->insert(BaseLib::StructElement("FILENAME", BaseLib::PVariable(new BaseLib::Variable((*i)->filename))));
					element->structValue->insert(BaseLib::StructElement("FAMILY_ID", BaseLib::PVariable(new BaseLib::Variable((*i)->familyId))));
					element->structValue->insert(BaseLib::StructElement("FAMILY_NAME", BaseLib::PVariable(new BaseLib::Variable((*i)->familyName))));
					element->structValue->insert(BaseLib::StructElement("BASELIB_VERSION", BaseLib::PVariable(new BaseLib::Variable((*i)->baselibVersion))));
					element->structValue->insert(BaseLib::StructElement("LOADED", BaseLib::PVariable(new BaseLib::Variable((*i)->loaded))));

					result->arrayValue->push_back(element);
				}

				return result;
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}

		BaseLib::PVariable ScriptEngineServer::loadModule(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 1) return BaseLib::Variable::createError(-1, "Method expects exactly one parameter.");
				if(parameters->at(0)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Parameter is not of type string.");
				if(parameters->at(0)->stringValue.empty()) return BaseLib::Variable::createError(-1, "Parameter is empty.");

				return BaseLib::PVariable(new BaseLib::Variable(GD::familyController->loadModule(parameters->at(0)->stringValue)));
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}

		BaseLib::PVariable ScriptEngineServer::unloadModule(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 1) return BaseLib::Variable::createError(-1, "Method expects exactly one parameter.");
				if(parameters->at(0)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Parameter is not of type string.");
				if(parameters->at(0)->stringValue.empty()) return BaseLib::Variable::createError(-1, "Parameter is empty.");

				return BaseLib::PVariable(new BaseLib::Variable(GD::familyController->unloadModule(parameters->at(0)->stringValue)));
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}

		BaseLib::PVariable ScriptEngineServer::reloadModule(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 1) return BaseLib::Variable::createError(-1, "Method expects exactly one parameter.");
				if(parameters->at(0)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Parameter is not of type string.");
				if(parameters->at(0)->stringValue.empty()) return BaseLib::Variable::createError(-1, "Parameter is empty.");

				return BaseLib::PVariable(new BaseLib::Variable(GD::familyController->reloadModule(parameters->at(0)->stringValue)));
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}
	// }}}

	// {{{ Licensing methods
		BaseLib::PVariable ScriptEngineServer::checkLicense(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 3 && parameters->size() != 4) return BaseLib::Variable::createError(-1, "Method expects three or four parameters. " + std::to_string(parameters->size()) + " given.");
				if(parameters->at(0)->type != BaseLib::VariableType::tInteger) return BaseLib::Variable::createError(-1, "Parameter 1 is not of type integer.");
				if(parameters->at(1)->type != BaseLib::VariableType::tInteger) return BaseLib::Variable::createError(-1, "Parameter 2 is not of type integer.");
				if(parameters->at(2)->type != BaseLib::VariableType::tInteger) return BaseLib::Variable::createError(-1, "Parameter 3 is not of type integer.");
				if(parameters->size() == 4 && parameters->at(3)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Parameter 4 is not of type string.");

				int32_t moduleId = parameters->at(0)->integerValue;
				int32_t familyId = parameters->at(1)->integerValue;
				int32_t deviceId = parameters->at(2)->integerValue;
				std::string licenseKey = (parameters->size() == 4) ? parameters->at(3)->stringValue : "";

				std::map<int32_t, std::unique_ptr<BaseLib::Licensing::Licensing>>::iterator i = GD::licensingModules.find(moduleId);
				if(i == GD::licensingModules.end() || !i->second)
				{
					_out.printError("Error: Could not check license. License module with id 0x" + BaseLib::HelperFunctions::getHexString(moduleId) + " not found");
					return BaseLib::PVariable(new BaseLib::Variable(-2));
				}

				return BaseLib::PVariable(new BaseLib::Variable(i->second->checkLicense(familyId, deviceId, licenseKey)));
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}

		BaseLib::PVariable ScriptEngineServer::removeLicense(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 3) return BaseLib::Variable::createError(-1, "Method expects exactly three parameters.");
				if(parameters->at(0)->type != BaseLib::VariableType::tInteger) return BaseLib::Variable::createError(-1, "Parameter 1 is not of type integer.");
				if(parameters->at(1)->type != BaseLib::VariableType::tInteger) return BaseLib::Variable::createError(-1, "Parameter 2 is not of type integer.");
				if(parameters->at(2)->type != BaseLib::VariableType::tInteger) return BaseLib::Variable::createError(-1, "Parameter 3 is not of type integer.");

				int32_t moduleId = parameters->at(0)->integerValue;
				int32_t familyId = parameters->at(1)->integerValue;
				int32_t deviceId = parameters->at(2)->integerValue;

				std::map<int32_t, std::unique_ptr<BaseLib::Licensing::Licensing>>::iterator i = GD::licensingModules.find(moduleId);
				if(i == GD::licensingModules.end() || !i->second)
				{
					_out.printError("Error: Could not check license. Licensing module with id 0x" + BaseLib::HelperFunctions::getHexString(moduleId) + " not found");
					return BaseLib::PVariable(new BaseLib::Variable(-2));
				}

				i->second->removeLicense(familyId, deviceId);

				return BaseLib::PVariable(new BaseLib::Variable(true));
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}

		BaseLib::PVariable ScriptEngineServer::getLicenseStates(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 1) return BaseLib::Variable::createError(-1, "Method expects exactly one parameter.");
				if(parameters->at(0)->type != BaseLib::VariableType::tInteger) return BaseLib::Variable::createError(-1, "Parameter 1 is not of type integer.");

				int32_t moduleId = parameters->at(0)->integerValue;

				std::map<int32_t, std::unique_ptr<BaseLib::Licensing::Licensing>>::iterator i = GD::licensingModules.find(moduleId);
				if(i == GD::licensingModules.end() || !i->second)
				{
					_out.printError("Error: Could not check license. Licensing module with id 0x" + BaseLib::HelperFunctions::getHexString(moduleId) + " not found");
					return BaseLib::PVariable(new BaseLib::Variable(false));
				}

				BaseLib::Licensing::Licensing::DeviceStates states = i->second->getDeviceStates();
				BaseLib::PVariable result(new BaseLib::Variable(BaseLib::VariableType::tArray));
				result->arrayValue->reserve(states.size());
				for(BaseLib::Licensing::Licensing::DeviceStates::iterator i = states.begin(); i != states.end(); ++i)
				{
					for(std::map<int32_t, BaseLib::Licensing::Licensing::PDeviceInfo>::iterator j = i->second.begin(); j != i->second.end(); ++j)
					{
						BaseLib::PVariable element(new BaseLib::Variable(BaseLib::VariableType::tStruct));

						element->structValue->insert(BaseLib::StructElement("MODULE_ID", BaseLib::PVariable(new BaseLib::Variable(j->second->moduleId))));
						element->structValue->insert(BaseLib::StructElement("FAMILY_ID", BaseLib::PVariable(new BaseLib::Variable(j->second->familyId))));
						element->structValue->insert(BaseLib::StructElement("DEVICE_ID", BaseLib::PVariable(new BaseLib::Variable(j->second->deviceId))));
						element->structValue->insert(BaseLib::StructElement("ACTIVATED", BaseLib::PVariable(new BaseLib::Variable(j->second->state))));
						element->structValue->insert(BaseLib::StructElement("LICENSE_KEY", BaseLib::PVariable(new BaseLib::Variable(j->second->licenseKey))));

						result->arrayValue->push_back(element);
					}
				}

				return result;
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}

		BaseLib::PVariable ScriptEngineServer::getTrialStartTime(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 3) return BaseLib::Variable::createError(-1, "Method expects three parameters. " + std::to_string(parameters->size()) + " given.");
				if(parameters->at(0)->type != BaseLib::VariableType::tInteger) return BaseLib::Variable::createError(-1, "Parameter 1 is not of type integer.");
				if(parameters->at(1)->type != BaseLib::VariableType::tInteger) return BaseLib::Variable::createError(-1, "Parameter 2 is not of type integer.");
				if(parameters->at(2)->type != BaseLib::VariableType::tInteger) return BaseLib::Variable::createError(-1, "Parameter 3 is not of type integer.");

				int32_t moduleId = parameters->at(0)->integerValue;
				int32_t familyId = parameters->at(1)->integerValue;
				int32_t deviceId = parameters->at(2)->integerValue;

				std::map<int32_t, std::unique_ptr<BaseLib::Licensing::Licensing>>::iterator i = GD::licensingModules.find(moduleId);
				if(i == GD::licensingModules.end() || !i->second)
				{
					_out.printError("Error: Could not check license. License module with id 0x" + BaseLib::HelperFunctions::getHexString(moduleId) + " not found");
					return BaseLib::PVariable(new BaseLib::Variable(-2));
				}

				return BaseLib::PVariable(new BaseLib::Variable(i->second->getTrialStartTime(familyId, deviceId)));
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}
	// }}}

	// {{{ Flows
		BaseLib::PVariable ScriptEngineServer::nodeOutput(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 3) return BaseLib::Variable::createError(-1, "Method expects three parameters. " + std::to_string(parameters->size()) + " given.");
				if(parameters->at(0)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Parameter 1 is not of type string.");
				if(parameters->at(1)->type != BaseLib::VariableType::tInteger) return BaseLib::Variable::createError(-1, "Parameter 2 is not of type integer.");

				GD::flowsServer->nodeOutput(parameters->at(0)->stringValue, parameters->at(1)->integerValue, parameters->at(2));

				return std::make_shared<BaseLib::Variable>();
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}

		BaseLib::PVariable ScriptEngineServer::executePhpNodeBaseMethod(PScriptEngineClientData& clientData, int32_t scriptId, BaseLib::PArray& parameters)
		{
			try
			{
				if(parameters->size() != 3) return BaseLib::Variable::createError(-1, "Method expects three parameters. " + std::to_string(parameters->size()) + " given.");
				if(parameters->at(0)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Parameter 1 is not of type string.");
				if(parameters->at(1)->type != BaseLib::VariableType::tString) return BaseLib::Variable::createError(-1, "Parameter 2 is not of type string.");
				if(parameters->at(2)->type != BaseLib::VariableType::tArray) return BaseLib::Variable::createError(-1, "Parameter 3 is not of type array.");

				return GD::flowsServer->executePhpNodeBaseMethod(parameters);
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			return BaseLib::Variable::createError(-32500, "Unknown application error.");
		}
	// }}}
// }}}

}

#endif
