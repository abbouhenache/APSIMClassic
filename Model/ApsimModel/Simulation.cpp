//---------------------------------------------------------------------------
#pragma hdrstop

#include <sstream>
#include <fstream>
#include <iostream>

#include <General/stl_functions.h>
#include <General/TreeNodeIterator.h>
#include <General/xml.h>
#include <General/path.h>

#include <ApsimShared/ApsimComponentData.h>
#include <ApsimShared/ApsimSystemData.h>
#include <ApsimShared/ApsimServiceData.h>
#include <ApsimShared/ApsimSimulationFile.h>
#include <ApsimShared/SimCreator.h>
#include <ApsimShared/ApsimDirectories.h>
#include <ApsimShared/ApsimRegistry.h>

#include <ComponentInterface/Interfaces.h>
#include <ComponentInterface/MessageDataExt.h>
#include <ComponentInterface/Messages.h>

#include <Protocol/Transport.h>
#include <Protocol/Computation.h>

#include "Simulation.h"

using namespace std;
using namespace protocol;

static const unsigned parentID = 1;
static const unsigned masterPMID = 1;

// ------------------------------------------------------------------
//  Short description:
//     constructor

//  Notes:

//  Changes:
//    dph 22/2/2000

// ------------------------------------------------------------------
Simulation::Simulation(void)
   {
   data = NULL;
   masterPM = NULL;
   initMessages();
   }

// ------------------------------------------------------------------
//  Short description:
//     destructor

//  Notes:

//  Changes:
//    dph 22/2/2000

// ------------------------------------------------------------------
Simulation::~Simulation(void)
   {
   deleteMessages();
   if (data != NULL)
      delete data;
   }

// ------------------------------------------------------------------
// Start the simulation.
// ------------------------------------------------------------------
void Simulation::go(const string& sdml)
   {
   data = new ApsimSimulationFile(sdml, true);
   init(sdml);
   commence();
   term();
   }   
// ------------------------------------------------------------------
//  Short description:
//    create the simulation.

//  Notes:

//  Changes:
//    dph 22/2/2000

// ------------------------------------------------------------------
void Simulation::init(const string& sdml)
   {
   // get dll filename for master PM.
   string dllFilename = data->getExecutableFileName();
   replaceAll(dllFilename, "%apsim%", getApsimDirectory());
   replaceAll(dllFilename, "%ausfarm%", getAusFarmDirectory());

   // create a Master PM and give it to the transport layer.
   string pmName = "";//".MasterPM";
   masterPM = new Computation(pmName, dllFilename, "", parentID, masterPMID);
   Transport::getTransport().addComponent(masterPMID, pmName, masterPM);

   // resolve any includes.
   string sdmlContents = sdml;
   resolveIncludes(sdmlContents);

   // initialise the simulation
   ApsimRegistry &registry = ApsimRegistry::getApsimRegistry();
   registry.addComponent(masterPMID, masterPMID, pmName, "protocolmanager", true);

   Message* message = constructMessage(Init1, parentID, masterPMID , false,
                                       memorySize(sdmlContents) + memorySize(pmName) + memorySize(true));
   MessageData messageData(message);
   messageData << sdmlContents << pmName << true;
   Transport::getTransport().deliverMessage(message);
   deleteMessage(message);

   // initialise the simulation
   message = constructMessage(Init2, parentID, masterPMID, false, 0);
   Transport::getTransport().deliverMessage(message);
   deleteMessage(message);
   }

// ------------------------------------------------------------------
//  Short description:
//     terminate the run

//  Notes:

//  Changes:
//    dph 22/2/2000

// ------------------------------------------------------------------
void Simulation::term(void)
   {
   if (masterPM != NULL)
      delete masterPM;
   }

// ------------------------------------------------------------------
//  Short description:
//    start this run

//  Notes:

//  Changes:
//    dph 22/2/2000

// ------------------------------------------------------------------
void Simulation::commence(void)
   {
   Message* message = newCommenceMessage(parentID, masterPMID);
   Transport::getTransport().deliverMessage(message);
   deleteMessage(message);
   }
// ------------------------------------------------------------------
//  Short description:
//    set the global log callback function

//  Notes:

//  Changes:
//    dph 22/2/2000

// ------------------------------------------------------------------
void Simulation::setMessageHook(IMessageHook* hook)
   {
   Transport::getTransport().setMessageHook(hook);
   }

// ------------------------------------------------------------------
// resolve any includes in the specified sdml.
// ------------------------------------------------------------------
void Simulation::resolveIncludes(string& sdml)
   {
   replaceAll(sdml, "%apsim%", getApsimDirectory());
   replaceAll(sdml, "%ausfarm%", getAusFarmDirectory());

   size_t posInclude = sdml.find("<include>");
   while (posInclude != string::npos)
      {
      size_t posFileName = posInclude + strlen("<include>");
      size_t posEndFileName = sdml.find("</include>", posFileName);
      if (posEndFileName == string::npos)
         throw runtime_error("Cannot find </include> tag");
      string includeFileName = sdml.substr(posFileName, posEndFileName-posFileName);
      string modelType = splitOffBracketedValue(includeFileName, '(', ')');

      if (! fileExists(includeFileName))
         throw runtime_error("Cannot find include file: " + includeFileName);
      string contents;
      if (includeFileName.find(".ini") != string::npos)
         {
         SimCreator simCreator(true);
         contents = simCreator.convertIniToSim(includeFileName);
         }
      else
         {
         XMLDocument doc(includeFileName);
         contents = findModelNode(doc.documentElement(), modelType);
         }

      size_t posEndInclude = posEndFileName + strlen("</include>");
      sdml.replace(posInclude, posEndInclude - posInclude, contents);

      posInclude = sdml.find("<include>");
      }
   }

// ------------------------------------------------------------------
// Go find the <Model> node under the specified node.
// ------------------------------------------------------------------
string Simulation::findModelNode(XMLNode Node, const string& modelType)
   {
   XMLNode ModelNode;
   if (modelType == "")
      ModelNode = findNode(Node, "Model");
   else
      ModelNode = findNodeWithName(Node, modelType);

   if (ModelNode.isValid())
      return ModelNode.innerXML();
   else
      return "";
   }


