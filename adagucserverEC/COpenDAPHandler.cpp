#include "COpenDAPHandler.h"
#include "CRequest.h"
#include "CDBFactory.h"
#include "CAutoResource.h"
const char * COpenDAPHandler::className = "COpenDAPHandler";

//References: http://opendap.org/pdf/ESE-RFC-004v1.2.pdf
//https://earthdata.nasa.gov/files/ESE-RFC-004v1.1.pdf

//http://opendap.knmi.nl/knmi/thredds/dodsC/ADAGUC/testsets/opendaptests/x4.nc
// wget "http://bhw485.knmi.nl:8080/cgi-bin/list.cgi/opendap/test2.nc.dods" -O /tmp/dat.txt && hexdump -C /tmp/dat.txt
//wget "http://opendap.knmi.nl/knmi/thredds/dodsC/ADAGUC/testsets/opendaptests/x4.nc.dods" -O /tmp/dat.txt && hexdump -C /tmp/dat.txt


//wget "http://bhw485.knmi.nl:8080/cgi-bin/list.cgi/opendap/test2.nc.dods?x" -O /tmp/dat.txt && hexdump -C /tmp/dat.txt
//wget "http://opendap.knmi.nl/knmi/thredds/dodsC/ADAGUC/testsets/opendaptests/x4.nc.dods?x" -O /tmp/dat.txt && hexdump -C /tmp/dat.txt

//wget --certificate /usr/people/plieger/impactspace/esg-dn1.nsc.liu.se.esgf-idp.openid.maartenplieger/certs/creds.pem --no-check-certificate https://bhw485.knmi.nl:8281/impactportal/DAP/esg-dn1.nsc.liu.se.esgf-idp.openid.maartenplieger/x4.nc.dods?x -O /tmp/dat.txt && hexdump -C /tmp/dat.txt

//#define COPENDAPHANDLER_DEBUG
class CDFTypeToOpenDAPType{
public:
  static CT::string getvar(const int type){
    CT::string rtype="unknown";
    if(type==CDF_NONE  )rtype=("CDF_NONE");
    if(type==CDF_BYTE  )rtype=("Byte");
    if(type==CDF_UBYTE )rtype=("Byte");
    if(type==CDF_CHAR  )rtype=("Byte");
    if(type==CDF_SHORT )rtype=("Int16");
    if(type==CDF_USHORT)rtype=("UInt16");
    if(type==CDF_INT   )rtype=("Int32");
    if(type==CDF_UINT  )rtype=("Uint32");
    if(type==CDF_FLOAT )rtype=("Float32");
    if(type==CDF_DOUBLE)rtype=("Float64");
    if(type==CDF_STRING)rtype=("String");
    return rtype;
  }
    static CT::string getatt(const int type){
    CT::string rtype="unknown";
    if(type==CDF_NONE  )rtype=("CDF_NONE");
    if(type==CDF_BYTE  )rtype=("Byte");
    if(type==CDF_UBYTE )rtype=("Byte");
    if(type==CDF_CHAR  )rtype=("String");
    if(type==CDF_SHORT )rtype=("Int16");
    if(type==CDF_USHORT)rtype=("UInt16");
    if(type==CDF_INT   )rtype=("Int32");
    if(type==CDF_UINT  )rtype=("Uint32");
    if(type==CDF_FLOAT )rtype=("Float32");
    if(type==CDF_DOUBLE)rtype=("Float64");
    if(type==CDF_STRING)rtype=("String");
    return rtype;
  }
};

    
int COpenDAPHandler::getDimSize(CDataSource *dataSource, const char *name){
  #ifdef COPENDAPHANDLER_DEBUG
  CDBDebug("getDimSize There are %d dims for %s",dataSource->cfgLayer->Dimension.size(),name);
  #endif
  //First check wether dims are configured in the DataBase
  for(size_t d=0;d<dataSource->cfgLayer->Dimension.size();d++){
    #ifdef COPENDAPHANDLER_DEBUG
    CDBDebug("getDimSize Checking : %s",dataSource->cfgLayer->Dimension[d]->attr.name.c_str());
    #endif
    if(dataSource->cfgLayer->Dimension[d]->attr.name.equals(name)){
      #ifdef COPENDAPHANDLER_DEBUG
      CDBDebug("getDimSize found : %s",dataSource->cfgLayer->Dimension[d]->attr.name.c_str());
      #endif
      CT::string tableName;
      CT::string dim = dataSource->cfgLayer->Dimension[d]->attr.name.c_str();
                
      try{
        tableName =  CDBFactory::getDBAdapter(dataSource->srvParams->cfg)->getTableNameForPathFilterAndDimension(dataSource->cfgLayer->FilePath[0]->value.c_str(),dataSource->cfgLayer->FilePath[0]->attr.filter.c_str(),dim.c_str(),dataSource);
      }catch(int e){
        CDBError("Unable to create tableName from '%s' '%s' '%s'",dataSource->cfgLayer->FilePath[0]->value.c_str(),dataSource->cfgLayer->FilePath[0]->attr.filter.c_str(), dim.c_str());
        return -1;
      }
      #ifdef COPENDAPHANDLER_DEBUG
      CDBDebug("getDimSize tableName = %s",tableName.c_str());
      #endif
      size_t dimSize = 0;
      CDBStore::Store *store = CDBFactory::getDBAdapter(dataSource->srvParams->cfg)->getUniqueValuesOrderedByValue(dim.c_str(),0,true,tableName.c_str());
      if(store!=NULL){
        if(store->size()!=0){
          #ifdef COPENDAPHANDLER_DEBUG
          CDBDebug("getDimSize %d",store->size());
          #endif
          dimSize = store->size();
        }
      }
      delete store;
      #ifdef COPENDAPHANDLER_DEBUG
      CDBDebug("getDimSize DimSize from DB for dim %s = %d",dim.c_str(),dimSize);
      #endif
      return dimSize;
    }
  }
  
  //Check wether we can find the dim in the netcdf file
  try{
    #ifdef COPENDAPHANDLER_DEBUG
    CDBDebug("getDimSize Trying to lookup in cdfObject");
    #endif
    CDF::Dimension *v= CDFObjectStore::getCDFObjectStore()->getCDFObjectHeaderPlain(dataSource->srvParams,dataSource->getFileName())->getDimension(name);
    #ifdef COPENDAPHANDLER_DEBUG
    CDBDebug("Length = %d",v->length);
    #endif
    return v->length;
  }catch(int e){
  }
  CDBError("getDimSize failed");
  return -1;
}


CT::string COpenDAPHandler::VarInfoToString(std::vector <VarInfo> selectedVariables){
  CT::string r;
  for(size_t j=0;j<selectedVariables.size();j++){
    r.printconcat("Variable Name: %s\n",selectedVariables[j].name.c_str());
    for(size_t i=0;i<selectedVariables[j].dimInfo.size();i++){
      r.printconcat("  Dim name %s :[%d %d %d]\n",selectedVariables[j].dimInfo[i].name.c_str(),selectedVariables[j].dimInfo[i].start,selectedVariables[j].dimInfo[i].count,selectedVariables[j].dimInfo[i].stride);  
    }
  }
  return r;
}

CT::string COpenDAPHandler::createDDSHeader(CT::string layerName, CDFObject *cdfObject ,std::vector <VarInfo> selectedVariables){
  /* Print DODS and DDS header */
  CT::string output = "";
  output.concat("Dataset {\n");
  for(size_t i=0;i<selectedVariables.size();i++){
    for(size_t j=0;j<cdfObject->variables.size();j++){
      CDF::Variable *v = cdfObject->variables[j];
      CDFType type = (CDFType)v->getType();

      if( selectedVariables[i].name.equals(&v->name)){
        output.printconcat("    %s ",CDFTypeToOpenDAPType::getvar(type).c_str());
        output.concat(v->name.c_str());
        for(size_t j=0;j<v->dimensionlinks.size();j++){
          int size  = -1;
          if(selectedVariables[i].dimInfo.size() == v->dimensionlinks.size()){
            size = selectedVariables[i].dimInfo[j].count;
          }
          output.printconcat("[%s = %d]",v->dimensionlinks[j]->name.c_str(),size);
        }
        if(v->dimensionlinks.size() == 0 ){
          output.printconcat("[%d]",1);
        }
        output.concat(";\n");
      }
    }
  }
  output.printconcat("} %s;\n",layerName.c_str());
  return output;
}



int bytesWritten = 0;
void writeInt(int v){
  unsigned char c1=((unsigned char)v);
  unsigned char c2=((unsigned char)(v>>8));
  unsigned char c3=((unsigned char)(v>>16));
  unsigned char c4=((unsigned char)(v>>24));;
//   c1 = 48;
//   c2=49 ; 
//   c3 = 50;
//   c4 = 51;
  fwrite(&c4,1,1,stdout);
  fwrite(&c3,1,1,stdout);
  fwrite(&c2,1,1,stdout);
  fwrite(&c1,1,1,stdout);
  bytesWritten+=4;
}

void writeDouble(double &v){
  unsigned char const * p = reinterpret_cast<unsigned char const *>(&v);
  fwrite(&p[7],1,1,stdout);
  fwrite(&p[6],1,1,stdout);
  fwrite(&p[5],1,1,stdout);
  fwrite(&p[4],1,1,stdout);
  fwrite(&p[3],1,1,stdout);
  fwrite(&p[2],1,1,stdout);
  fwrite(&p[1],1,1,stdout);
  fwrite(&p[0],1,1,stdout);

  bytesWritten+=8;
}

int COpenDAPHandler::putVariableDataSize(CDF::Variable *v){
 
  if(v->getType() != CDF_STRING){
    int a = v->getSize();
    writeInt(a);
    writeInt(a);
  }else{

     int a = v->getSize();
    
    writeInt(a);
  }
  
  
  return 0;
}

// int tdata = 48;
int COpenDAPHandler::putVariableData(CDF::Variable *v,CDFType type){
  int written = 0;
  size_t typeSize = CDF::getTypeSize(type);
  

  
  size_t varSize = v->getSize();
  //CDBDebug("name:%s typeSize:%d varSize:%d",v->name.c_str(),typeSize,varSize);
  if(type==CDF_BYTE||
    type==CDF_UBYTE||
    type==CDF_CHAR||
    type==CDF_INT||
    type==CDF_UINT||
    type==CDF_FLOAT||
    type==CDF_DOUBLE){
    unsigned char *data = (unsigned char*)v->data;
    for(size_t d=0;d<varSize;d++){
      for(size_t e=0;e<typeSize;e++){
        putc(data[d*typeSize+(typeSize-1)-e],stdout);
        //putc(tdata,stdout);
        
      }
      bytesWritten+=typeSize;
      written+=typeSize;
    }
  }
  
  if(type==CDF_SHORT||
    type==CDF_USHORT){
    unsigned char *data = (unsigned char*)v->data;

    for(size_t d=0;d<varSize;d++){
      putc(0,stdout);
      putc(0,stdout);
      bytesWritten+=2;
      written+=2;
      for(size_t e=0;e<typeSize;e++){
        
    
        putc(data[d*typeSize+(typeSize-1)-e],stdout);

        //putc(tdata,stdout);
        
      }
      bytesWritten+=typeSize;
      written+=typeSize;
    }
  }
  
  //Strings need to be '0' terminated.
  if(type==CDF_STRING){
    const char **data = (const char**)v->data;
    for(size_t d=0;d<varSize;d++){
      size_t l = strlen(data[d]);
      writeInt(l);
      for(size_t e=0;e<l;e++){
        //CDBDebug("%s",data[d][e]);
        
        putc(data[d][e],stdout);
        written++;
        bytesWritten++;
        
      }
      
      //Padding bytes to sequences of four.
      while(int(written/4)*4 !=written){
        putc(0,stdout);
        written++;
        bytesWritten++;
      }
    }
  }


  //Bytes need to be padded to words of four bytes
  if((type==CDF_BYTE||type==CDF_CHAR||type==CDF_UBYTE))
  {
    //Padding bytes to sequences of four.
    while(int(written/4)*4 !=written){
      CDBDebug("Padding");
      putc(48,stdout);
      written++;
      bytesWritten++;
    }
  }
  //fflush(stdout);
  return 0;
}

int COpenDAPHandler::HandleOpenDAPRequest(const char *path,const char *query,CServerParams *srvParam){
 
  #ifdef COPENDAPHANDLER_DEBUG
  CDBDebug("\n*****************************************************************************************");
 
  #endif
  CDBDebug("OpenDAP Received [%s] [%s]",path,query);
  CT::string dapName = path+8;
  CT::string layerName  = "";
  CT::string pathQuery = "";
  bool isDDSRequest = false;
  bool isDASRequest = false;
  bool isDODRequest = false;
  dapName.decodeURLSelf();
  
  //CDBDebug("dapName: %s",dapName.c_str());

  //dapName.replaceSelf("%20"," ");
  int i = dapName.lastIndexOf(".dds");
  if(i != -1){
    layerName = dapName.substring(0,i);
    pathQuery = dapName.substring(i+4,-1);
    isDDSRequest = true;
  }else{
    int i = dapName.lastIndexOf(".das");
    if(i != -1){
      layerName = dapName.substring(0,i);
      pathQuery = dapName.substring(i+4,-1);
      isDASRequest = true;
    }else{
      int i = dapName.lastIndexOf(".dods");
      if(i != -1){
        layerName = dapName.substring(0,i);
        pathQuery = dapName.substring(i+5,-1);
        isDODRequest = true;
      }else{
        int i = dapName.lastIndexOf(".dds");
        if(i != -1){
          layerName = dapName.substring(0,i);
          pathQuery = dapName.substring(i+5,-1);
          isDDSRequest = true;
        }
      }
    }
  }
  
  if(isDDSRequest == false && isDASRequest == false && isDODRequest == false){
    CDBError("Not a valid OpenDAP request received, e.g. use .dds, .das");
    return 1;
  }
  
  //Check if a dataset/dataURL was given
  CT::string dataURL = "";
  int lastSlash = layerName.lastIndexOf("/");
  if(lastSlash !=-1){
    dataURL = layerName.substring(0,lastSlash);
    layerName = layerName.substring(lastSlash+1,-1);
    
    
  }
  
/*  
  CDBDebug("dataURL: %s",dataURL.c_str());
  CDBDebug("layerName: %s",layerName.c_str());*/
  
  if(dataURL.length()>0){
    srvParam->autoResourceLocation = dataURL;
    if(CAutoResource::configure(srvParam,true)!=0){
      CDBError("AutoResource failed");
      return 1;
    }
  }
  
  #ifdef COPENDAPHANDLER_DEBUG
  CDBDebug("Layername = %s",layerName.c_str());
  CDBDebug("pathQuery = %s",pathQuery.c_str());
  #endif
  CDataSource *dataSource = new CDataSource ();
  bool foundLayer = false;
  
  for(size_t layerNo=0;layerNo<srvParam->cfg->Layer.size();layerNo++){
    CT::string intLayerName;
    srvParam->makeUniqueLayerName(&intLayerName,srvParam->cfg->Layer[layerNo]);
    if(intLayerName.equals(layerName.c_str())){
      if(dataSource->setCFGLayer(srvParam,srvParam->configObj->Configuration[0],srvParam->cfg->Layer[layerNo],intLayerName.c_str(),0)!=0){
        CDBError("Error setCFGLayer");
        return 1;
      }
      foundLayer = true;
      break;
    }
  }
  
  if(foundLayer == false){
    CDBError("Unable to find layer %s",layerName.c_str());
    delete dataSource;
    return 1;
//     CDFObject *cdfObject = CDFObjectStore::getCDFObjectStore()->getCDFObjectHeaderPlain(srvParam,srvParam->internalAutoResourceLocation.c_str());
//     CDF::Variable *v = cdfObject->getVariableNE(layerName.c_str());
//     if(v== NULL){
//       CDBError("Unable to find layer %s",layerName.c_str());
//       delete dataSource;
//       return 1;
//     }else{
// //       if(dataSource->setCFGLayer(srvParam,srvParam->configObj->Configuration[0],srvParam->cfg->Layer[layerNo],intLayerName.c_str(),0)!=0){
// //         CDBError("Error setCFGLayer");
// //         return 1;
// //       }
// //       foundLayer = true;
//     }
  }
      
      if(isDODRequest){
        printf("%s%c%c\n","Content-Type: application/octet-stream",13,10);
      }else{
        printf("%s%c%c\n","Content-Type: text/plain",13,10);
      }
#ifdef COPENDAPHANDLER_DEBUG      
CDBDebug("Found layer %s",layerName.c_str());
  #endif
  if(dataSource->dLayerType==CConfigReaderLayerTypeDataBase||
      dataSource->dLayerType==CConfigReaderLayerTypeStyled)
  {
    //When this layer has no dimensions, we do not need to query 
    // When there are no dims, we can get the filename from the config
    if(dataSource->cfgLayer->Dimension.size()==0){
      
      if(CDataReader::autoConfigureDimensions(dataSource)!=0){
        CDBError("Unable to configure dimensions automatically");
        return 1;
      }
    }
//     if(dataSource->cfgLayer->Dimension.size()!=0){
//       if(CRequest::getDimValuesForDataSource(dataSource,srvParam)!=0){
//         CDBError("Unable to fill in dimensions");
//         return 1;
//       }
//     }else{
      //This layer has no dimensions, but we need to add one timestep with data in order to make the next code work.        
      #ifdef COPENDAPHANDLER_DEBUG      
      CDBDebug("This layer has no dims, adding one virtual time step.");
      #endif
      CDirReader dirReader;
      if(CDBFileScanner::searchFileNames(&dirReader,dataSource->cfgLayer->FilePath[0]->value.c_str(),dataSource->cfgLayer->FilePath[0]->attr.filter,NULL)!=0){
        CDBError("Could not find any filename");
        return 1; 
      }
      if(dirReader.fileList.size()==0){
        CDBError("dirReader.fileList.size()==0");return 1;
      }
      dataSource->addStep(dirReader.fileList[0]->fullName.c_str(),NULL);
      dataSource->getCDFDims()->addDimension("time","0",0);
//     }
  }
  
  
  
  
  //Read the NetCDF header!
  
  
  try{
  
//    CDataReader reader;
//     int status = reader.open(dataSource,CNETCDFREADER_MODE_OPEN_HEADER);
//     if(status!=0){
//       CDBError("Could not open file: %s",dataSource->getFileName());
//       throw(__LINE__);
//     }
    
    

    
    CDFObject *cdfObject =  CDFObjectStore::getCDFObjectStore()->getCDFObjectHeaderPlain(dataSource->srvParams,dataSource->getFileName());;// dataSource->getDataObject(0)->cdfObject;
      
    for(size_t d=0;d<dataSource->cfgLayer->Dimension.size();d++){
      //Check for the configured dimensions or scalar variables
      //1 )Is this a scalar?
      CDF::Variable *  dimVar = cdfObject->getVariableNE(dataSource->cfgLayer->Dimension[d]->attr.name.c_str());
      CDF::Dimension * dimDim = cdfObject->getDimensionNE(dataSource->cfgLayer->Dimension[d]->attr.name.c_str());
      
      
      if(dimVar!=NULL&&dimDim==NULL){
        //Check for scalar variable
        if(dimVar->dimensionlinks.size() == 0){
          #ifdef COPENDAPHANDLER_DEBUG
          CDBDebug("Found scalar variable %s with no dimension. Creating dim",dimVar->name.c_str());
          #endif
          dimDim = new CDF::Dimension();
          dimDim->name = dimVar->name;
          dimDim->setSize(1);
          cdfObject->addDimension(dimDim);
          dimVar->dimensionlinks.push_back(dimDim);
        }

      }
    }
    
    
    if(isDDSRequest||isDODRequest){
     
      
      std::vector <VarInfo> selectedVariables;
      //Parsing dim queries per variable (e.g. precip[0][0:3] == x,y)
      if(strlen(query)>0){
        CT::string q= query;
        CT::StackList<CT::string> items=q.splitToStack(",");
        for(size_t j=0;j<items.size();j++){
          #ifdef COPENDAPHANDLER_DEBUG
          CDBDebug("Selected variable = \"%s\"",items[j].c_str());
          #endif
          
          //Split on every [ token, gives sequences precip, 0] and 0:3]
          CT::StackList<CT::string> varsettings=items[j].splitToStack("[");
          varsettings[0].decodeURLSelf();
         
          //Push the variable
          #ifdef COPENDAPHANDLER_DEBUG
          CDBDebug("Push varinfo %s",varsettings[0].c_str());
          #endif
          
          selectedVariables.push_back(VarInfo(varsettings[0].c_str()));
          //Retrieve other settings, like start,count,stride
          if(varsettings.size() > 1){
            //Fill in start/count/stride from request
            #ifdef COPENDAPHANDLER_DEBUG
            CDBDebug("Getting start/count/stride from request");
            #endif
            for(size_t d=1;d<varsettings.size();d++){
              varsettings[d].replaceSelf("]",""); //gives sequences precip, 0 and 0:3
              
              //Now split on :
              CT::StackList<CT::string> startCountStrideItems = varsettings[d].splitToStack(":");
              size_t start = 0;
              size_t count = 1;
              size_t stride = 1;
              if(startCountStrideItems.size()==1){
                start = startCountStrideItems[0].toInt();
              }
              if(startCountStrideItems.size()==2){
                start = startCountStrideItems[0].toInt();
                count = startCountStrideItems[1].toInt()-start; count++;
               
              }
              if(startCountStrideItems.size()==3){//TODO CHECK if [start:count:stride] is correct.
                start = startCountStrideItems[0].toInt();
                count = startCountStrideItems[1].toInt()-start; count++;
                stride= startCountStrideItems[2].toInt();
              }
              #ifdef COPENDAPHANDLER_DEBUG
              CDBDebug("DIMINFO: %d,%s  %d:%d",start,varsettings[d].c_str(),d,j);
              #endif
              CT::string dimname = cdfObject->getVariable(selectedVariables.back().name.c_str())->dimensionlinks[d-1]->name.c_str();
              #ifdef COPENDAPHANDLER_DEBUG
              CDBDebug("Push dimInfo %s",dimname.c_str());
              #endif
              selectedVariables.back().dimInfo.push_back(VarInfo::Dim(dimname.c_str(),start,count,stride));
            }
          }
        }
      }
      
    
      //If no variables where selected, select them all.
      if(selectedVariables.size() == 0){
        #ifdef COPENDAPHANDLER_DEBUG
        CDBDebug("Selecting all variables");
        #endif
        for(size_t j=0;j<cdfObject->variables.size();j++){
          #ifdef COPENDAPHANDLER_DEBUG
          CDBDebug("Push varinfo %s",cdfObject->variables[j]->name.c_str());
          #endif
          //if(cdfObject->variables[j]->name.equals(layerName)){
            selectedVariables.push_back(VarInfo(cdfObject->variables[j]->name.c_str()));
          //}
        }
      }
      
      #ifdef COPENDAPHANDLER_DEBUG
      CDBDebug("Getting start/count/stride from database");
      #endif
      for(size_t i=0;i<selectedVariables.size();i++){
        if( selectedVariables[i].dimInfo.size() == 0){
          CT::string varname = selectedVariables[i].name.c_str();
          
          try{
            CDF::Variable *v = cdfObject->getVariable(varname.c_str());

            for(size_t j=0;j<v->dimensionlinks.size();j++){
              int size = v->dimensionlinks[j]->getSize();
              int dimSize = getDimSize(dataSource,v->dimensionlinks[j]->name.c_str());
              #ifdef COPENDAPHANDLER_DEBUG
              CDBDebug("Getting DimSize %d",dimSize);
              #endif
              if(dimSize > 0){
                size = dimSize;
              }
              size_t count = size;
              size_t stride = 1;
              size_t start = 0;
              #ifdef COPENDAPHANDLER_DEBUG
              CDBDebug("Push dimInfo varinfo %s",v->dimensionlinks[j]->name.c_str());
              #endif
              selectedVariables[i].dimInfo.push_back(VarInfo::Dim(v->dimensionlinks[j]->name.c_str(),start,count,stride));
            }
          }catch(int e){
            CDBError("Exception %s",CDF::getErrorMessage(e).c_str());
          }
        }
      }
      
      
      
//       for (std::vector<VarInfo>::iterator it = selectedVariables.begin() ; it != selectedVariables.end(); ++it){
//         if(it->name.equals("crs")==true
// 
//         ){
//           selectedVariables.erase(it);
//           it--;
//         }
//       }

      
      
      
      #ifdef COPENDAPHANDLER_DEBUG
      CT::string r = VarInfoToString(selectedVariables);
      
      CDBDebug("selectedVariables:[\n%s",r.c_str());
      CDBDebug("]");
      #endif
      
      CT::string output = createDDSHeader(layerName,cdfObject,selectedVariables);
      printf("%s\r\n",output.c_str());
      //fflush(stdout);
      
      CDFObject* cdfObjectToRead = NULL;
      //Data request
      if(isDODRequest){
        //printf("0000000000"); 
        printf("Data:\r\n"); 
        //fflush(stdout);
        for(size_t i=0;i<selectedVariables.size();i++){
          for(size_t j=0;j<cdfObject->variables.size();j++){
            CDF::Variable *v = cdfObject->variables[j];
            CDFType type = (CDFType)v->getType();

            if( selectedVariables[i].name.equals(&v->name)){
              #ifdef COPENDAPHANDLER_DEBUG
              CDBDebug("selectedVariables[i].dimInfo.size() = %d",selectedVariables[i].dimInfo.size());
              CDBDebug("v->dimensionlinks.size()  = %d",v->dimensionlinks.size() );
              #endif
              bool hasAggregateDimension = false;
              //Check wether we need to iterate or not
              for(size_t k=0;k<dataSource->requiredDims.size();k++){
                for(size_t l=0;l<selectedVariables[i].dimInfo.size();l++){
                  if(dataSource->requiredDims[k]->netCDFDimName.equals(selectedVariables[i].dimInfo[l].name.c_str())){
                    hasAggregateDimension = true;break;
                  }
                }
              }
              if(hasAggregateDimension){
                size_t start[ v->dimensionlinks.size()];
                size_t count[ v->dimensionlinks.size()];
                ptrdiff_t stride[ v->dimensionlinks.size()];
                for(size_t k=0;k< v->dimensionlinks.size();k++){
                  start[k] = selectedVariables[i].dimInfo[k].start;
                  count[k] = selectedVariables[i].dimInfo[k].count;
                  stride[k] = selectedVariables[i].dimInfo[k].stride;
                }
                //Convert start/count/stride to database request.
                
                
                #ifdef COPENDAPHANDLER_DEBUG
                CDBDebug("Starting reading partial data over aggregation dimension");
                #endif
                size_t varSize = 1;
                #ifdef COPENDAPHANDLER_DEBUG
                CDBDebug("Start retrieving files for variable %s",v->name.c_str());
                #endif
                for(size_t j=0;j< v->dimensionlinks.size();j++){
                  #ifdef COPENDAPHANDLER_DEBUG
                  CDBDebug(" start[%d:%s] = %d %d %d",j, v->dimensionlinks[j]->name.c_str(),start[j],count[j],stride[j]);
                  #endif
                  varSize*=count[j];
                }
                
                bool foundData = false;
                CDBStore::Store *store = NULL;
                try{
                  store = CDBFactory::getDBAdapter(dataSource->srvParams->cfg)->getFilesForIndices(dataSource,start,count,stride,0);
                  if(store!=NULL){
                    #ifdef COPENDAPHANDLER_DEBUG
                    CDBDebug("STORE SIZE %d varSize = %d",store->size(),varSize);
                    #endif
                    if(store->size()!=0){
                      writeInt(varSize);
                      writeInt(varSize);
                      
                      foundData = true;
                      
                      CT::string dimStandardName = "";
                      try{
                        dimStandardName = v->getAttribute("standard_name")->getDataAsString();;
                      }catch(int e){
                      }
                      CT::string dimUnits = "";
                      try{
                        dimUnits = v->getAttribute("units")->getDataAsString();;
                      }catch(int e){
                      }
                      bool readFromDB=false;
                      //If variable name equals dimension name, values are stored in the database.
                      CTime time;
                      if(v->isDimension&&type==CDF_DOUBLE){
                        if(v->dimensionlinks.size()==1){
                          if(v->name.equals(v->dimensionlinks[0]->name)){
                            
                            
                            
                            
                           
                            if(dimStandardName.equals("time")&&dimUnits.length()>2){
                              readFromDB=true;
                              
                              time.init(dimUnits.c_str());
                         
                            }
                            //                               CDBDebug("%s",store->getRecord(storeIndex)->get(0)->c_str());
                            //                               CDBDebug("%s",store->getRecord(storeIndex)->get(1)->c_str());//value
                            //                               CDBDebug("%s",store->getRecord(storeIndex)->get(2)->c_str());
                          }
                        }
                      }
                      for(size_t storeIndex=0;storeIndex<store->size();storeIndex++){
                        CT::string fileName = store->getRecord(storeIndex)->get(0)->c_str();
                        #ifdef COPENDAPHANDLER_DEBUG
                        CDBDebug("Found file %s",fileName.c_str());
                        #endif
                        cdfObjectToRead = CDFObjectStore::getCDFObjectStore()->getCDFObjectHeaderPlain(dataSource->srvParams,fileName.c_str());
                        start[0]=store->getRecord(storeIndex)->get(2)->toInt();
                        count[0]=1;
                        #ifdef COPENDAPHANDLER_DEBUG
                        CDBDebug("Start reading data for variable %s",v->name.c_str());
                        for(size_t j=0;j< v->dimensionlinks.size();j++){
                          CDBDebug("  start[%d] = %d %d %d",j,start[j],count[j],stride[j]);
                        }
                        
                        #endif
                        if(readFromDB){
                          CT::string dimValue = store->getRecord(storeIndex)->get(1);
                       
                          
                          double value = time.dateToOffset(time.freeDateStringToDate(dimValue.c_str()));
                          #ifdef COPENDAPHANDLER_DEBUG
                          CDBDebug("Dimension value from DB = [%s] units = [%s] standard_name = [%s]",dimValue.c_str(),dimUnits.c_str(),dimStandardName.c_str());
                          //CDBDebug("Convert value %f",value);
                          #endif
                          writeDouble(value);
                        }

                        if(readFromDB == false){
                          CDF::Variable *variableToRead = cdfObjectToRead->getVariable(v->name.c_str());
                          variableToRead->readData(type,start,count,stride);
                          #ifdef COPENDAPHANDLER_DEBUG
                          CDBDebug("Read %d elements with type %s with element size %d",variableToRead->getSize(),CDF::getCDFDataTypeName(type).c_str(), CDF::getTypeSize(type));
                          #endif

                      
                          putVariableData(variableToRead,type);
                        }
                      }
                    }
                  }
                  delete store;
                }catch(int e){
                  delete store;
                  CDBError("Exception %s in getFilesForIndices",CDF::getErrorMessage(e).c_str());
                }
                

                if(foundData == false){
                  #ifdef COPENDAPHANDLER_DEBUG
                  CDBDebug("Read all data for %s",v->name.c_str());
                  #endif
                  int status = v->readData(type);
                  if(status!=0){
                    CDBError("Unable to read data for %s",v->name.c_str());
                    return -1;
                  }else{
                    putVariableDataSize(v);
                    putVariableData(v,type);
                  }
                }
                
                
              }else{
                // No aggregate dimension
                #ifdef COPENDAPHANDLER_DEBUG
                CDBDebug("Read data for %s",v->name.c_str());
                #endif
                int status = 0;
                if( v->dimensionlinks.size()>0){
                  #ifdef COPENDAPHANDLER_DEBUG
                  CDBDebug("READ PARTS");
                  #endif
                  size_t start[ v->dimensionlinks.size()];
                  size_t count[ v->dimensionlinks.size()];
                  ptrdiff_t stride[ v->dimensionlinks.size()];
                  for(size_t k=0;k< v->dimensionlinks.size();k++){
                    start[k] = selectedVariables[i].dimInfo[k].start;
                    count[k] = selectedVariables[i].dimInfo[k].count;
                    stride[k] = selectedVariables[i].dimInfo[k].stride;
                  }
                  #ifdef COPENDAPHANDLER_DEBUG
                  CDBDebug("Variable %s",v->name.c_str());
                  for(size_t j=0;j< v->dimensionlinks.size();j++){
                            CDBDebug(" start[%d] = %d %d %d",j,start[j],count[j],stride[j]);
                          }
                  #endif
                    //v->freeData();
                    status = v->readData(type,start,count,stride);
//                     if(v->getType()==CDF_STRING){
//                       for(size_t j=0;j<v->getSize();j++){
//                         CDBDebug("%s",((char**)v->data)[j]);
//                       }
//                     }
                }else{
                  #ifdef COPENDAPHANDLER_DEBUG
                  CDBDebug("READ ALL");
                  #endif
               
                  v->readData(type);
                }
                if(status!=0){
                  CDBError("Unable to read data for %s",v->name.c_str());
                  return -1;
                }else{
                  putVariableDataSize(v);
                  putVariableData(v,type);
                }
              }
              
              
              
//               //Strings need to be null terminated
//               if(type == CDF_CHAR){
//                 CDBDebug("ADDING CHAR for %s",v->name.c_str());
//                 putc(0,stdout);
//                 putc(0,stdout);
//                 putc(0,stdout);
//                 
//               }
            }
          }
        }
        //fflush(stdout);
      }
     
    }
    
    if(isDASRequest){
      CDFObject *cdfObject =  CDFObjectStore::getCDFObjectStore()->getCDFObjectHeaderPlain(dataSource->srvParams,dataSource->getFileName());
      CT::string output = "";
      output.concat("Attributes {\n");
      for(size_t j=0;j<cdfObject->variables.size();j++){
        CDF::Variable *v = cdfObject->variables[j];
        //if(v->name.equals("custom")==false)
        {
        output.printconcat("    %s {\n",v->name.c_str());
        for(size_t j=0;j<v->attributes.size();j++){
          //if(v->attributes[j]->name.charAt(0)!='_'&&v->attributes[j]->type!=CDF_DOUBLE)
          {
            CT::string attrName = v->attributes[j]->name;
            attrName.replaceSelf(" ","_");
            attrName.replaceSelf("\"","_");
            attrName.replaceSelf("[","_");
            attrName.replaceSelf("]","_");
            output.printconcat("        %s %s ",CDFTypeToOpenDAPType::getatt(v->attributes[j]->type).c_str(),attrName.c_str());
            if(v->attributes[j]->type == CDF_CHAR){
              output.concat("\"");
              CT::string s = v->attributes[j]->getDataAsString().c_str();
            
              //s.encodeURLSelf();
//               s.replaceSelf(":","");
//               s.replaceSelf("[","");
              s.replaceSelf("\"","\\\"");
              
              output.concat(s.c_str());
              if(v->attributes[j]->type == CDF_CHAR)output.concat("\"");
            }else{
              CT::string s= v->attributes[j]->getDataAsString();
              s.replaceSelf(" ",",");
              output.concat(s.c_str());
            }
            output.printconcat(";\n",v->attributes[j]->name.c_str());
          }
        }
        output.concat("    }\n");
      }
      }
      output.printconcat("}");
      printf("%s\n",output.c_str());
    }

    
//    reader.close();
  }catch(int e){
  }
  
  
  delete dataSource;
  #ifdef COPENDAPHANDLER_DEBUG
  CDBDebug("**************************** OPENDAP END *******************************");
  #endif
  return 0;
}
