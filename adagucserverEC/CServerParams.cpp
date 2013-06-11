/******************************************************************************
 * 
 * Project:  ADAGUC Server
 * Purpose:  ADAGUC OGC Server
 * Author:   Maarten Plieger, plieger "at" knmi.nl
 * Date:     2013-06-01
 *
 ******************************************************************************
 *
 * Copyright 2013, Royal Netherlands Meteorological Institute (KNMI)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 ******************************************************************************/

#include "CServerParams.h"
const char *CServerParams::className="CServerParams";

CServerParams::CServerParams(){
  
  WMSLayers=NULL;
  serviceType=-1;
  requestType=-1;
  OGCVersion=-1;

  Transparent=false;
  enableDocumentCache=false;
  configObj = new CServerConfig();
  Geo = new CGeoParams;
  imageFormat=IMAGEFORMAT_IMAGEPNG8;
  imageMode=SERVERIMAGEMODE_8BIT;
  autoOpenDAPEnabled=-1;
  autoLocalFileResourceEnabled = -1;
  autoResourceCacheEnabled = -1;
  showDimensionsInImage = false;
  showLegendInImage = false;
  figWidth=-1;
  figHeight=-1;
}

CServerParams::~CServerParams(){
  if(WMSLayers!=NULL){delete[] WMSLayers;WMSLayers=NULL;}
  if(configObj!=NULL){delete configObj;configObj=NULL;}
  if(Geo!=NULL){delete Geo;Geo=NULL;}
  for(size_t j=0;j<requestDims.size();j++){
    delete requestDims[j];
    requestDims[j]= NULL;
  }
  requestDims.clear();
  
}

void CServerParams::getCacheFileName(CT::string *cacheFileName){
  CT::string cacheName("WMSCACHE");
  bool useProvidedCacheFileName=false;
  //Check wether a cachefile has been provided in the config
  if(cfg->CacheDocs.size()>0){
    if(cfg->CacheDocs[0]->attr.cachefile.c_str()!=NULL){
      useProvidedCacheFileName=true;
      cacheName.concat("_");
      cacheName.concat(cfg->CacheDocs[0]->attr.cachefile.c_str());
    }
  }
  if(useProvidedCacheFileName==false){
    //If no cache filename is provided, we will create a standard one
    //Based on the name of the configuration file
    cacheName.concat(&configFileName);
    for(size_t j=0;j<cacheName.length();j++){
      char c=cacheName.charAt(j);
      if(c=='/')c='_';
      if(c=='\\')c='_';
      if(c=='.')c='_';
      cacheName.setChar(j,c);
    }
  }
  

  //Insert the tmp dir in the beginning
  cacheFileName->copy(cfg->TempDir[0]->attr.value.c_str());
  cacheFileName->concat("/");
  cacheFileName->concat(&cacheName);
//  CDBDebug("Make pub dir %s",cacheFileName->c_str());
  CDirReader::makePublicDirectory(cacheFileName->c_str());
  cacheFileName->concat("/simplecachestore.dat");
}


void CServerParams::getCacheDirectory(CT::string *cacheFileName){
  //CDBDebug("getCacheDirectory");
  CT::string cacheName("WMSCACHE");
  bool useProvidedCacheFileName=false;
  //Check wether a cachefile has been provided in the config
  if(cfg->CacheDocs.size()>0){
    if(cfg->CacheDocs[0]->attr.cachefile.c_str()!=NULL){
      useProvidedCacheFileName=true;
      cacheName.concat("_");
      cacheName.concat(cfg->CacheDocs[0]->attr.cachefile.c_str());
    }
  }
  if(useProvidedCacheFileName==false){
    //If no cache filename is provided, we will create a standard one
    //Based on the name of the configuration file
    cacheName.concat(&configFileName);
    for(size_t j=0;j<cacheName.length();j++){
      char c=cacheName.charAt(j);
      if(c=='/')c='_';
      if(c=='\\')c='_';
      if(c=='.')c='_';
      cacheName.setChar(j,c);
    }
  }
  //append .dat extension
  //cacheName.concat(".dat");
  //Insert the tmp dir in the beginning
  cacheFileName->copy(cfg->TempDir[0]->attr.value.c_str());
  cacheFileName->concat("/");
  cacheFileName->concat(&cacheName);
}

#include <ctime>
#include <sys/time.h>
#include <stdio.h>
#define USE_DEVRANDOM



const CT::string randomString(const int len) {
    char s[len+1];
    
    timeval curTime;
    gettimeofday(&curTime, NULL);
    
#ifdef USE_DEVRANDOM
    FILE* randomData = fopen("/dev/urandom", "r");
    int myRandomInteger;
    fread(&myRandomInteger, sizeof myRandomInteger,1,randomData);
    srand(myRandomInteger);
    fclose(randomData);
#else
    srand((curTime.tv_sec * 1000) + (curTime.tv_usec / 1000));
#endif
    
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
    CT::string r=s;
    return r;
}

CT::string currentDateTime() {
    timeval curTime;
    gettimeofday(&curTime, NULL);
    int milli = curTime.tv_usec / 1000;

    char buffer [80];
    strftime(buffer, 80, "%Y-%m-%dT%H:%M:%S", localtime(&curTime.tv_sec));

    char currentTime[84] = "";
    sprintf(currentTime, "%s:%03dZ", buffer, milli);
   

    return currentTime;
}

CT::string CServerParams::lookupTableName(const char *path,const char *filter, const char * dimension){
  CT::string tableName;
 
  // This makes use of a lookup table to find the tablename belonging to the filter and path combinations.
  // Database collumns: path filter tablename
  
  CT::string filterString="F_";filterString.concat(filter);
  CT::string pathString="P_";pathString.concat(path);
  CT::string dimString="";if(dimension != NULL){dimString.concat(dimension);dimString.toLowerCaseSelf();}
  
  CT::string lookupTableName = "pathfiltertablelookup";
  CT::string tableColumns("path varchar (511), filter varchar (511), dimension varchar (511), tablename varchar (63), UNIQUE (path,filter,dimension) ");
  CT::string mvRecordQuery;
  int status;
  CPGSQLDB DB;
  const char *pszDBParams = this->cfg->DataBase[0]->attr.parameters.c_str();
  status = DB.connect(pszDBParams);if(status!=0){
    CDBError("Error Could not connect to the database with parameters: [%s]",pszDBParams);
    throw(1);
  }

  status = DB.checkTable(lookupTableName.c_str(),tableColumns.c_str());
  //if(status == 0){CDBDebug("OK: Table %s is available",lookupTableName.c_str());}
  if(status == 1){CDBError("\nFAIL: Table %s could not be created: %s",lookupTableName.c_str(),tableColumns.c_str()); DB.close();throw(1);  }
  //if(status == 2){CDBDebug("OK: Table %s is created",lookupTableName.c_str());  }

  
  //Check wether a records exists with this path and filter combination.
  
  bool lookupTableIsAvailable=false;
  
  
  
  if(dimString.length()>1){
    mvRecordQuery.print("SELECT * FROM %s where path=E'%s' and filter=E'%s' and dimension=E'%s'",
                        lookupTableName.c_str(),pathString.c_str(),filterString.c_str(),dimString.c_str());
  }else{
    lookupTableIsAvailable=true;
    mvRecordQuery.print("SELECT * FROM %s where path=E'%s' and filter=E'%s'",
                        lookupTableName.c_str(),pathString.c_str(),filterString.c_str());
  }
  CDB::Store *rec = DB.queryToStore(mvRecordQuery.c_str()); 
  if(rec==NULL){CDBError("Unable to select records: \"%s\"",mvRecordQuery.c_str());DB.close();throw(1);  }
  if(rec->getSize()>0){
    tableName.copy(rec->getRecord(0)->get(3));
    
    lookupTableIsAvailable = true;
    
  }
  /*if(rec->getSize()>1){
    CDBError("Path filter combination has more than 1 lookuptable");
    delete rec;
    DB.close();
    throw(1);
  }*/
  delete rec;
  
  //Add a new lookuptable with an unique id.
  if(lookupTableIsAvailable==false){
    CT::string randomTableString = "t";
    randomTableString.concat(currentDateTime());
    randomTableString.concat("_");
    randomTableString.concat(randomString(20));
    randomTableString.replaceSelf(":","");
    randomTableString.replaceSelf("-","");
    randomTableString.replaceSelf("Z",""); 
    
    tableName.copy(randomTableString.c_str());
    mvRecordQuery.print("INSERT INTO %s values (E'%s',E'%s',E'%s',E'%s')",lookupTableName.c_str(),pathString.c_str(),filterString.c_str(),dimString.c_str(),tableName.c_str());
    CDBDebug("%s",mvRecordQuery.c_str());
    status = DB.query(mvRecordQuery.c_str()); if(status!=0){CDBError("Unable to insert records: \"%s\"",mvRecordQuery.c_str());DB.close();throw(1);  }
  }
  //Close the database
  DB.close();
  return tableName;
}


//Table names need to be different between dims like time and height.
// Therefore create unique tablenames like tablename_time and tablename_height
void CServerParams::makeCorrectTableName(CT::string *tableName,CT::string *dimName){
  tableName->concat("_");tableName->concat(dimName);
  tableName->replaceSelf("-","_m_");
  tableName->replaceSelf("+","_p_");
  tableName->replaceSelf(".","_");
  tableName->toLowerCaseSelf();
}

void CServerParams::showWCSNotEnabledErrorMessage(){
  CDBError("WCS is not enabled because GDAL was not compiled into the server. ");
}


int CServerParams::makeUniqueLayerName(CT::string *layerName,CServerConfig::XMLE_Layer *cfgLayer){
  /*
  if(cfgLayer->Variable.size()!=0){
  _layerName=cfgLayer->Variable[0]->value.c_str();
}*/
  
  layerName->copy("");
  if(cfgLayer->Group.size()==1){
    if(cfgLayer->Group[0]->attr.value.c_str()!=NULL){
      layerName->copy(cfgLayer->Group[0]->attr.value.c_str());
      layerName->concat("/");
    }
  }
  if(cfgLayer->Name.size()==0){
    CServerConfig::XMLE_Name *name=new CServerConfig::XMLE_Name();
    cfgLayer->Name.push_back(name);
    if(cfgLayer->Variable.size()==0){
      name->value.copy("undefined_variable");
    }else{
      name->value.copy(cfgLayer->Variable[0]->value.c_str());
    }
  }
  
  if(!cfgLayer->Name[0]->attr.force.equals("true")){
    layerName->concat(cfgLayer->Name[0]->value.c_str());
    layerName->replaceSelf(".","_");
  }else{
    layerName->copy(cfgLayer->Name[0]->value.c_str());
  }
  
  
  /*if(cfgLayer->Title.size()==0){
    CServerConfig::XMLE_Title *title=new CServerConfig::XMLE_Title();
    cfgLayer->Title.push_back(title);
    title->value.copy(cfgLayer->Name[0]->value.c_str());
  } */ 
  
  
  

  return 0;
}

bool CServerParams::isAutoResourceCacheEnabled(){
  if(autoResourceCacheEnabled==-1){
     autoResourceCacheEnabled = 1;
    if(cfg->AutoResource.size()>0){
      if(!cfg->AutoResource[0]->attr.enablecache.equals("true"))autoResourceCacheEnabled = 0;
    }
  }
  if(autoResourceCacheEnabled==0)return false;
  return true;
}


bool CServerParams::isAutoOpenDAPResourceEnabled(){
  if(autoOpenDAPEnabled==-1){
    autoOpenDAPEnabled = 0;
    if(cfg->AutoResource.size()>0){
      if(cfg->AutoResource[0]->attr.enableautoopendap.equals("true"))autoOpenDAPEnabled = 1;
    }
  }
  if(autoOpenDAPEnabled==0)return false;else return true;
  return false;
}

bool CServerParams::isAutoLocalFileResourceEnabled(){
  if(autoLocalFileResourceEnabled==-1){
    autoLocalFileResourceEnabled = 0;
    if(cfg->AutoResource.size()>0){
      if(cfg->AutoResource[0]->attr.enablelocalfile.equals("true"))autoLocalFileResourceEnabled = 1;
    }
  }
  if(autoLocalFileResourceEnabled==0)return false;else return true;
  return false;
}

bool CServerParams::isAutoResourceEnabled(){
  if(isAutoOpenDAPResourceEnabled()||isAutoLocalFileResourceEnabled())return true;
  return false;
}


bool CServerParams::checkIfPathHasValidTokens(const char *path){
  //Check for valid tokens
  const char *validPATHTokens="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789/_-+:. ";
  size_t pathLength=strlen(path);
  size_t allowedTokenLength=strlen(validPATHTokens);
  for(size_t j=0;j<pathLength;j++){
    bool isInvalid = true;
    for(size_t i=0;i<allowedTokenLength;i++){
      if(path[j]==validPATHTokens[i]){isInvalid=false;break;}
    }
    if(isInvalid){
      CDBDebug("Invalid token '%c' in '%s'",path[j],path);
      return false;
    }
    
    //Check for sequences
    if(j>0){
      if(path[j-1]=='.'&&path[j]=='.'){
        CDBDebug("Invalid sequence in '%s'",path);
        return false;
      }
    }
  }
  return true;
}


bool CServerParams::checkResolvePath(const char *path,CT::string *resolvedPath){
  if(cfg->AutoResource.size()>0){
    //Needs to be configured otherwise it will be denied.
    if(cfg->AutoResource[0]->Dir.size()==0){
      CDBDebug("No Dir elements defined");
      return false;
    }
    
    if(checkIfPathHasValidTokens(path)==false)return false;
    
    for(size_t d=0;d<cfg->AutoResource[0]->Dir.size();d++){
      const char *baseDir =cfg->AutoResource[0]->Dir[d]->attr.basedir.c_str();
      const char *dirPrefix=cfg->AutoResource[0]->Dir[d]->attr.prefix.c_str();
      
      if(baseDir!=NULL&&dirPrefix!=NULL){
        //Prepend the prefix to make the absolute path
        CT::string pathToCheck;
        pathToCheck.print("%s/%s",dirPrefix,path);
        
        //Make a realpath
        char szResolvedPath[PATH_MAX];
        if(realpath(pathToCheck.c_str(),szResolvedPath)==NULL){
          //CDBDebug("basedir='%s', prefix='%s', inputpath='%s', absolutepath='%s'",baseDir,dirPrefix,path,pathToCheck.c_str());
          CDBDebug("LOCALFILEACCESS: Invalid path '%s'",pathToCheck.c_str());
        }else{
          //Check if the resolved path is within the basedir
          //CDBDebug("basedir='%s', prefix='%s', inputpath='%s', absolutepath='%s'",baseDir,dirPrefix,path,pathToCheck.c_str());
          CT::string resolvedPathStr=szResolvedPath;
          if(resolvedPathStr.indexOf(baseDir)==0){
            resolvedPath->copy(szResolvedPath);
            return true;
          }
        }
      }else{
        if(baseDir==NULL){CDBDebug("basedir not defined");}
        if(dirPrefix==NULL){CDBDebug("prefix not defined");}
      }
    }
  }else{
    CDBDebug("No AutoResource enabled");
  }
  return false;
}

void CServerParams::encodeTableName(CT::string *tableName){
  tableName->replaceSelf("/","_");
  tableName->toLowerCaseSelf();
}



CT::string CServerParams::getOnlineResource(){
  CT::string onlineResource=cfg->OnlineResource[0]->attr.value.c_str();
  //A full path is given
  if(onlineResource.indexOf("http",4)==0){
    return onlineResource;
  }
  //Only the last part is given, we need to prepend the HTTP_HOST environment variable.
  const char *pszHTTPHost=getenv("HTTP_HOST");
  if(pszHTTPHost==NULL){
    CDBError("Unable to determine HTTP_HOST");
    return "";
  }
  CT::string httpHost="http://";
  httpHost.concat(pszHTTPHost);
  httpHost.concat(&onlineResource);
  return httpHost;
}
