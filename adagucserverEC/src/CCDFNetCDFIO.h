#ifndef CCDFNETCDFIO_H
#define CCDFNETCDFIO_H
#include "CCDFDataModel.h"
#include <stdio.h>
#include <vector>
#include <iostream>
#include <netcdf.h>
#include <math.h>
#include "CDebugger.h"
//#include "CStopWatch.h"

#define CCDFNETCDFIO_ENABLELONWARP false
void ncError(int line, const char *className, const char * msg,int e);
class CDFNetCDFReader :public CDFReader{
  private:
    bool lonWarpNeeded;size_t lonWarpStartIndex;
  CDFType typeConversion(nc_type type){
    if(type==NC_BYTE)return CDF_BYTE;
    if(type==NC_UBYTE)return CDF_UBYTE;
    if(type==NC_CHAR)return CDF_CHAR;
    if(type==NC_SHORT)return CDF_SHORT;
    if(type==NC_USHORT)return CDF_USHORT;
    if(type==NC_INT)return CDF_INT;
    if(type==NC_UINT)return CDF_UINT;
    if(type==NC_FLOAT)return CDF_FLOAT;
    if(type==NC_DOUBLE)return CDF_DOUBLE;
    return CDF_DOUBLE;
  }
  DEF_ERRORFUNCTION();
  int status,root_id;
  int nDims,nVars,nRootAttributes,unlimDimIdP;    
  int readDimensions(){
    char name[NC_MAX_NAME+1];
    size_t length;
    for(int j=0;j<nDims;j++){
      status = nc_inq_dim(root_id,j,name,&length);
      if(status!=NC_NOERR){ncError(__LINE__,className,"nc_inq_dim: ",status);return 1;}
      CDF::Dimension * existingDim = cdfObject->getDimension(name);
      //Only add non existing variables;
      if(existingDim!=NULL){
        if(existingDim->length!=length){
          CDBError("Previously dimensions size for dim %s is not the same as new definition",name);
          return 1;
        }
      }else{
        CDF::Dimension * dim = new CDF::Dimension();
        dim->id=j;
        dim->setName(name);
        dim->length=length;
        cdfObject->dimensions.push_back(dim);
      }
    }
    return 0;
  }
  int readAttributes(std::vector<CDF::Attribute *> &attributes,int varID,int natt){
    char name[NC_MAX_NAME+1];
    nc_type type;
    size_t length;
    for(int i=0;i<natt;i++){
      status = nc_inq_attname(root_id,varID,i,name);
      if(status!=NC_NOERR){ncError(__LINE__,className,"nc_inq_attname: ",status);return 1;}
      //Only add non existing attributes;
      int attributeExists=false;
      for(size_t k=0;k<attributes.size();k++){if(attributes[k]->name.equals(name)){attributeExists=true;break;}}
      if(attributeExists==false){
        status = nc_inq_att(root_id,varID,name,&type,&length);
        if(status!=NC_NOERR){ncError(__LINE__,className,"nc_inq_att: ",status);return 1;}
        CDF::Attribute *attr = new CDF::Attribute();
        attr->setName(name);
        attr->type=typeConversion(type);
        attr->length=length;
        CDF::allocateData(attr->type,&attr->data,attr->length+1);
        status = nc_get_att(root_id,varID,name,attr->data);
        if(type==NC_CHAR)((char*)attr->data)[attr->length]='\0';
        if(status!=NC_NOERR){ncError(__LINE__,className,"nc_get_att: ",status);return 1;}
        attributes.push_back(attr);
      }
    }
    return 0;
  }
  int readVariables(){
    char name[NC_MAX_NAME+1];
    nc_type type;
    int ndims;
    int natt;
    int dimids[NC_MAX_VAR_DIMS];
    bool isDimension;
    for(int j=0;j<nVars;j++){
      status = nc_inq_var(root_id,j,name,&type,&ndims,dimids,&natt);
      if(status!=NC_NOERR){ncError(__LINE__,className,"nc_inq_var: ",status);return 1;}
      //Only add non existing variables...
      CDF::Variable* existingVariable = cdfObject->getVariable(name);
      if(existingVariable==NULL){
        //printf("%s\n",name);
        CDF::Variable * var = new CDF::Variable();
        isDimension = false;
          //Is this a dimension:
        for(size_t i=0;i<cdfObject->dimensions.size();i++){if(cdfObject->dimensions[i]->name.equals(name)){isDimension=true;break;}}
          //Dimension links:
        for(int k=0;k<ndims;k++){
          for(size_t i=0;i<cdfObject->dimensions.size();i++){
            if(cdfObject->dimensions[i]->id==dimids[k]){
              var->dimensionlinks.push_back(cdfObject->dimensions[i]);
              break;
            }
          }
        }
        var->type=typeConversion(type);
        var->setName(name);
        var->id=j;
        var->isDimension=isDimension;
          //Attributes:
        status = readAttributes(var->attributes,j,natt);if(status!=0)return 1;
        cdfObject->variables.push_back(var);
          //printf("%d: %s %d\n",j,var->name.c_str(),isDimension);
        //It is essential that the variable nows which reader can be used to read the data
        var->cdfReaderPointer=(void*)this;
      }
    }
    return 0;
  }

  public:
    CDFNetCDFReader(CDFObject *cdfObject):CDFReader(cdfObject){
      root_id=-1;
      lonWarpNeeded=false;
      lonWarpStartIndex = 0;
    }
    ~CDFNetCDFReader(){
      close();
    }
    
    int open(const char *fileName){
      // Check type sizes 
      if(sizeof(char)  !=1){CDBError("The size of char is unequeal to 8 Bits");return 1;}
      if(sizeof(short) !=2){CDBError("The size of short is unequeal to 16 Bits");return 1;}
      if(sizeof(int)   !=4){CDBError("The size of int is unequeal to 32 Bits");return 1;}
      if(sizeof(float) !=4){CDBError("The size of float is unequeal to 32 Bits");return 1;}
      if(sizeof(double)!=8){CDBError("The size of double is unequeal to 64 Bits");return 1;}
      //CDBDebug("opening %s",fileName);
      status = nc_open(fileName,NC_NOWRITE,&root_id);
      if(status!=NC_NOERR){ncError(__LINE__,className,"nc_open: ",status);return 1;}
/*#ifdef MEASURETIME
          StopWatch_Stop("CDFNetCDFReader open file\n");
#endif*/
      status = nc_inq(root_id,&nDims,&nVars,&nRootAttributes,&unlimDimIdP);
      if(status!=NC_NOERR){ncError(__LINE__,className,"nc_inq: ",status);return 1;}
/*#ifdef MEASURETIME
          StopWatch_Stop("NC_INQ");
#endif*/

      status = readDimensions();if(status!=0)return 1;
/*#ifdef MEASURETIME
          StopWatch_Stop("readDim");
#endif*/
      
      status = readVariables();if(status!=0)return 1;
/*#ifdef MEASURETIME
          StopWatch_Stop("readVar");
#endif*/
      
      status = readAttributes(cdfObject->attributes,NC_GLOBAL,nRootAttributes);if(status!=0)return 1;
/*#ifdef MEASURETIME
          StopWatch_Stop("readAttr");
#endif*/
      
      return 0;
    }
    int close(){
      
      if(root_id!=-1){
        nc_close(root_id);
      }
      root_id=-1;
      return 0;
    }
    
    int warpLonData(CDF::Variable *variable){
      //Apply longitude warping of the data
      //Longitude data must be already present in order to make variable warping available.
      //EG 0-360 to -180 till -180
      bool enableLonWarp=CCDFNETCDFIO_ENABLELONWARP;
      if(enableLonWarp){
        if(variable->name.equals("lon")){
          //CDBDebug("Warplon: Found variable lon");
          double *lonData = new double[variable->getSize()];
          CDF::dataCopier.copy(lonData,variable->data,variable->type,variable->getSize());
          double average=0;
          double cellSize = fabs(lonData[0]-lonData[1]);
          for(size_t j=0;j<variable->getSize();j++){
            average+=lonData[j];
          }
          average/=double(variable->getSize());
          //CDBDebug("Warplon: average = %f",average);
          if(average>180-cellSize&&average<180+cellSize)lonWarpNeeded=true;
          if(lonWarpNeeded==true){
            for(size_t j=0;j<variable->getSize()&&(lonData[j]<=180);j++){
              lonWarpStartIndex=j;
            }
                        //printf("%d==%f %f\n",lonWarpStartIndex,lonData[lonWarpStartIndex]-360,lonData[variable->getSize()-1]-360);
            //CDBDebug("Warplon: start warp");
                        //Warp longitude:
            for(size_t j=0;j<variable->getSize();j++){
              lonData[j]-=180;
              if(variable->type==CDF_FLOAT)((float*)variable->data)[j]=float(lonData[j]);
              if(variable->type==CDF_DOUBLE)((double*)variable->data)[j]=double(lonData[j]);
            }
          }
          delete[] lonData;
        }
        if(lonWarpNeeded==true){
          int dimIndex = variable->getDimensionIndex("lon");
          if(dimIndex!=-1){
            //CDBDebug("Warplon: Found dimension lon for variable %s",variable->name.c_str());
            if(dimIndex!=((int)variable->dimensionlinks.size())-1){
              CDBError("Error while warping longitude dimension for variable %s: longitude is not the first index",variable->name.c_str());
              return 1;
            }
            CDF::Dimension *dim = variable->getDimension("lon");
            if(dim!=NULL){
              size_t offset=0;
              do{
                for(size_t lon=0;lon<lonWarpStartIndex&&(lon+lonWarpStartIndex<dim->length);lon++){
                  if(variable->type==CDF_FLOAT){
                    float *data=(float*)variable->data;
                    size_t p1=lon+offset;
                    size_t p2=lon+lonWarpStartIndex+offset;
                    float tmp1=data[p1];
                    float tmp2=data[p2];
                    data[p1]=tmp2;
                    data[p2]=tmp1;
                  }
                }
                offset+=dim->length;
              }while(offset<variable->getSize());
            }else{
              CDBError("Warplon: No dimension lon found for variable %s",variable->name.c_str());
            }
          }
        }
      }
      return 0;
    }
    
    int readVariableData(CDF::Variable *var, CDFType type){
      //CDBDebug("readVariableData");
      //It is essential that the variable nows which reader can be used to read the data
      var->cdfReaderPointer=(void*)this;
     size_t totalVariableSize = 1;
      for(size_t i=0;i<var->dimensionlinks.size();i++){
        totalVariableSize*=var->dimensionlinks[i]->length;
      }
      var->setSize(totalVariableSize);
      CDF::allocateData(type,&var->data,var->getSize());
      if(type==CDF_BYTE)status = nc_get_var_ubyte(root_id,var->id,(unsigned char*)var->data);
      else if(type==CDF_CHAR)status = nc_get_var_text(root_id,var->id,(char*)var->data);
      else if(type==CDF_SHORT)status = nc_get_var_short(root_id,var->id,(short*)var->data);
      else if(type==CDF_INT)status = nc_get_var_int(root_id,var->id,(int*)var->data);
      else if(type==CDF_FLOAT)status = nc_get_var_float(root_id,var->id,(float*)var->data);
      else if(type==CDF_DOUBLE)status = nc_get_var_double(root_id,var->id,(double*)var->data);
      else {
        CDBError("Unable to determine netcdf type\n");
        return 1;
      }
      if(status!=NC_NOERR){
        CDBError("Problem with variable %s:",var->name.c_str());
        ncError(__LINE__,className,"nc_get_var: ",status);
        return 1;}
        warpLonData(var);
      return 0;
    }
    
    int readVariableData(CDF::Variable *var, CDFType type,size_t *start,size_t *count,ptrdiff_t *stride){
      //CDBDebug("readVariableData");
      //It is essential that the variable nows which reader can be used to read the data
      var->cdfReaderPointer=(void*)this;
      size_t totalVariableSize = 1;
      bool useStriding=false;
      for(size_t i=0;i<var->dimensionlinks.size();i++){
        totalVariableSize*=count[i];//stride[i];
        if(stride[i]!=1)useStriding=true;
      }
      //CDBDebug("totalVariableSize = %d",totalVariableSize);
      var->setSize(totalVariableSize);
      CDF::allocateData(type,&var->data,var->getSize());
      //CDBDebug("Stride enabled: %d",useStriding);
      if(!useStriding){
        
        if(type==CDF_BYTE)status = nc_get_vara_ubyte(root_id,var->id,start,count,(unsigned char*)var->data);
        else if(type==CDF_CHAR)status = nc_get_vara_text(root_id,var->id,start,count,(char*)var->data);
        else if(type==CDF_SHORT)status = nc_get_vara_short(root_id,var->id,start,count,(short*)var->data);
        else if(type==CDF_INT)status = nc_get_vara_int(root_id,var->id,start,count,(int*)var->data);
        else if(type==CDF_FLOAT)status = nc_get_vara_float(root_id,var->id,start,count,(float*)var->data);
        else if(type==CDF_DOUBLE)status = nc_get_vara_double(root_id,var->id,start,count,(double*)var->data);
        else {
          CDBError("Unable to determine netcdf type for variable %s",var->name.c_str());
          return 1;
        }
      }else{
        if(type==CDF_BYTE||type==CDF_UBYTE)status = nc_get_vars_ubyte(root_id,var->id,start,count,stride,(unsigned char*)var->data);
        else if(type==CDF_CHAR)status = nc_get_vars_text(root_id,var->id,start,count,stride,(char*)var->data);
        else if(type==CDF_SHORT)status = nc_get_vars_short(root_id,var->id,start,count,stride,(short*)var->data);
        else if(type==CDF_INT)status = nc_get_vars_int(root_id,var->id,start,count,stride,(int*)var->data);
        else if(type==CDF_FLOAT)status = nc_get_vars_float(root_id,var->id,start,count,stride,(float*)var->data);
        else if(type==CDF_DOUBLE)status = nc_get_vars_double(root_id,var->id,start,count,stride,(double*)var->data);
        else {
          CDBError("Unable to determine netcdf type for variable %s",var->name.c_str());
          return 1;
        }
      }
      if(status!=NC_NOERR){ncError(__LINE__,className,"nc_get_var: ",status);return 1;}
      
      warpLonData(var);
      
      return 0;
    }

};


class CDFNetCDFWriter{
  private:
    nc_type NCtypeConversion(CDFType  type){
      if(type==CDF_BYTE)return NC_BYTE;
      if(type==CDF_UBYTE)return NC_UBYTE;
      if(type==CDF_CHAR)return NC_CHAR;
      if(type==CDF_SHORT)return NC_SHORT;
      if(type==CDF_USHORT)return NC_USHORT;
      if(type==CDF_INT)return NC_INT;
      if(type==CDF_UINT)return NC_UINT;
      if(type==CDF_FLOAT)return NC_FLOAT;
      if(type==CDF_DOUBLE)return NC_DOUBLE;
      return NC_DOUBLE;
    }
    bool writeData;
    bool readData;
  public:
    std::vector<CDF::Dimension *> dimensions;
    CDFObject *cdfObject;
    
    DEF_ERRORFUNCTION();
    int root_id,status;
    int netcdfMode;
    CDFNetCDFWriter(CDFObject *cdfObject){
      this->cdfObject=cdfObject;
      writeData=true;
      readData=true;
      netcdfMode=4;
    }
    ~CDFNetCDFWriter(){
      for(size_t j=0;j<dimensions.size();j++){delete dimensions[j];}
    }
    void setNetCDFMode(int mode){
      if(mode!=3&&mode!=4){
        CDBError("Illegal netcdf mode %d: keeping mode ",mode,netcdfMode);
        return;
      }
      netcdfMode=mode;
    }
    void disableVariableWrite(){
      writeData=false;
    }
    void disableReadData(){
      readData=false;
    }

    int write(const char *fileName){
      if(netcdfMode>3){
        status = nc_create(fileName , NC_NETCDF4|NC_CLOBBER, &root_id);
      }else{
        status = nc_create(fileName , NC_CLOBBER, &root_id);
      }
      if(status!=NC_NOERR){ncError(__LINE__,className,"nc_create: ",status);nc_close(root_id);return 1;}
      status = _write();
      nc_close(root_id);
      return status;
    }
    int _write(){
      //Write global attributes
      for(size_t i=0;i<cdfObject->attributes.size();i++){
        status = nc_put_att(root_id, NC_GLOBAL, cdfObject->attributes[i]->name.c_str(),
                            NCtypeConversion(cdfObject->attributes[i]->type),cdfObject->attributes[i]->length,
                            cdfObject->attributes[i]->data);
        if(status!=NC_NOERR){
          char name[1023];
          CDF::getCDFDataTypeName(name,1000,cdfObject->attributes[i]->type);
          CDBError("For attribute NC_GLOBAL::%s of type %s:",cdfObject->attributes[i]->name.c_str(),name);
          ncError(__LINE__,className,"nc_put_att: ",status);return 1;
        }
      }
      //Define dimensions
      for(size_t j=0;j<cdfObject->dimensions.size();j++){
        CDF::Dimension * dim = new CDF::Dimension();
        dim->setName(cdfObject->dimensions[j]->name.c_str());
        dim->length=cdfObject->dimensions[j]->length;
        status = nc_def_dim(root_id,dim->name.c_str() , dim->length, &dim->id);
        if(status!=NC_NOERR){ncError(__LINE__,className,"nc_def_dim: ",status);return 1;}
        dimensions.push_back(dim);
      }
   
      bool enableLonWarp = CCDFNETCDFIO_ENABLELONWARP; //To be set by user
      bool lonWarpNeeded = false; //Internal variable
      size_t lonWarpStartIndex = 0;
      int nrVarsWritten=0;
      //Write the variables
      //First write the variables connected to dimensions and later the variables itself
      //writeDimsFirst==0: dimension variables
      //writeDimsFirst==1: variables
      for(int writeDimsFirst=0;writeDimsFirst<2;writeDimsFirst++){
        for(size_t j=0;j<cdfObject->variables.size();j++){
          //Get the variable names with these dimensions 
          CDF::Variable *variable = cdfObject->variables[j];
          const char *name = variable->name.c_str();
          int numDims=variable->dimensionlinks.size();
          if((variable->isDimension==true &&writeDimsFirst==0)||
             (variable->isDimension==false&&writeDimsFirst==1)){
            {
              int dimIDS[numDims+1];
              size_t chunkSizes[numDims+1];
              size_t totalVariableSize = 0;
              //Find dim and chunk info
              CT::string variableInfo(name);
              variableInfo.concat("\t(");
              for(int i=0;i<numDims;i++){
                for(size_t k=0;k<dimensions.size();k++){
                  if(dimensions[k]->name.equals(&variable->dimensionlinks[i]->name)){
                    dimIDS[i]=dimensions[k]->id;
                    if(totalVariableSize==0)totalVariableSize=1;
                    //CDBDebug("EQUALS: %s %d",dimensions[k]->name.c_str(),dimIDS[i]);  
                    totalVariableSize*=dimensions[k]->length;
                    chunkSizes[i]=dimensions[k]->length;
                    variableInfo.printconcat("%s=%d",dimensions[k]->name.c_str(),dimensions[k]->length);
                    if(i+1<numDims)variableInfo.concat(",");
                  }
                }
              }
              variableInfo.concat(")");
              int nc_var_id;
              status = nc_redef(root_id);
              /*for(int j=0;j<numDims;j++){
                printf("%d\n",dimIDS[j]);
            }*/
              status = nc_def_var(root_id, name    ,NCtypeConversion(variable->type),numDims, dimIDS,&nc_var_id);
              if(status!=NC_NOERR){ncError(__LINE__,className,"nc_def_var: ",status);return 1;}
              //Set chunking and deflate options
              chunkSizes[0]=1;
              if(netcdfMode>=4&&numDims>0&&1==1){
                int shuffle       = 0;
                int deflate       = 1;
                int deflate_level = 2;
               // status = nc_def_var_chunking(root_id,nc_var_id,0 ,chunkSizes);
                //if(status!=NC_NOERR){ncError(__LINE__,className,"nc_def_var_chunking: ",status);return 1;}
                status = nc_def_var_deflate(root_id,nc_var_id,shuffle ,deflate, deflate_level);
                if(status!=NC_NOERR){ncError(__LINE__,className,"nc_def_var_deflate: ",status);return 1;}
                
              }
              
              //copy data
              CT::string message;
              message.print("%d/%d Copying data for variable %s: total %d bytes",
                            nrVarsWritten+1,cdfObject->variables.size(),variableInfo.c_str(),int(totalVariableSize)*CDF::getTypeSize(variable->type));
              CDBDebug("%s",message.c_str());
              //Copy attributes for this specific variable
              for(size_t i=0;i<variable->attributes.size();i++){
                if(!variable->attributes[i]->name.equals("CLASS")){
                  nc_type type=NCtypeConversion(variable->attributes[i]->type);
                  status = nc_put_att(root_id, nc_var_id, variable->attributes[i]->name.c_str(),
                                      type,variable->attributes[i]->length,
                                      variable->attributes[i]->data);
                  if(status!=NC_NOERR){
                    char attrType[256],varType[256];
                    CDF::getCDFDataTypeName(attrType,255,variable->attributes[i]->type);
                    CDF::getCDFDataTypeName(varType,255,variable->type);
                    CDBError("Trying to write attribute %s with type %s for variable %s with type %s\nnc_put_att: %s",
                            variable->attributes[i]->name.c_str(),attrType,variable->name.c_str(),varType,nc_strerror(status));return 1;
                  }
                }else {
                  //CDBDebug("Skipping attribute %s:%s",variable->name.c_str(),variable->attributes[i]->name.c_str());
                }
              }
              if(numDims>0&&writeData==true){
                
                if(readData==true){
                  CDFReader *cdfReader = (CDFReader *)variable->cdfReaderPointer;
                  if(cdfReader!=NULL){
                    status = cdfReader->readVariableData(variable, variable->type);
                    if(status!=0){
                      CDBError("Reading of variable %s failed",variable->name.c_str());
                      return 1;
                    }
                  }else{
                    if(variable->data==NULL){
                      CDBError("No cdfReaderPointer defined for variable %s and no data specified",variable->name.c_str());
                      return 1;
                    }else{
                      //CDBWarning("No cdfReaderPointer defined for variable %s",variable->name.c_str());
                    }
                  }
                  
                  //Apply longitude warping of the data
                  //EG 0-360 to -180 till -180
                  
                  if(enableLonWarp){
                    if(variable->name.equals("lon")){
                      //CDBDebug("Warplon: Found variable lon");
                      double *lonData = new double[variable->getSize()];
                      CDF::dataCopier.copy(lonData,variable->data,variable->type,variable->getSize());
                      double average=0;
                      double cellSize = fabs(lonData[0]-lonData[1]);
                      for(size_t j=0;j<variable->getSize();j++){
                        average+=lonData[j];
                      }
                      average/=double(variable->getSize());
                      //CDBDebug("Warplon: average = %f",average);
                      if(average>180-cellSize&&average<180+cellSize)lonWarpNeeded=true;
                      if(lonWarpNeeded==true){
                        for(size_t j=0;j<variable->getSize()&&(lonData[j]<=180);j++){
                            lonWarpStartIndex=j;
                        }
                        //printf("%d==%f %f\n",lonWarpStartIndex,lonData[lonWarpStartIndex]-360,lonData[variable->getSize()-1]-360);
                        //CDBDebug("Warplon: start warp");
                        //Warp longitude:
                        for(size_t j=0;j<variable->getSize();j++){
                          lonData[j]-=180;
                          if(variable->type==CDF_FLOAT)((float*)variable->data)[j]=float(lonData[j]);
                          if(variable->type==CDF_DOUBLE)((double*)variable->data)[j]=double(lonData[j]);
                        }
                      }
                      delete[] lonData;
                    }
                    if(lonWarpNeeded==true){
                      int dimIndex = variable->getDimensionIndex("lon");
                      if(dimIndex!=-1){
                        if(dimIndex!=((int)variable->dimensionlinks.size())-1){
                          CDBError("Error while warping longitude dimension for variable %s: longitude is not the first index",variable->name.c_str());
                          return 1;
                        }
                        CDF::Dimension *dim = variable->getDimension("lon");
                        if(dim!=NULL){
                          size_t offset=0;
                          do{
                            for(size_t lon=0;lon<lonWarpStartIndex&&(lon+lonWarpStartIndex<dim->length);lon++){
                              if(variable->type==CDF_FLOAT){
                                float *data=(float*)variable->data;
                                size_t p1=lon+offset;
                                size_t p2=lon+lonWarpStartIndex+offset;
                                float tmp1=data[p1];
                                float tmp2=data[p2];
                                data[p1]=tmp2;
                                data[p2]=tmp1;
                              }
                            }
                            offset+=dim->length;
                          }while(offset<variable->getSize());
                        }
                      }
                    }
                  }
                }
                if(status==0){
                  if(variable->data==NULL){
                    CDBError("variable->data==NULL for %s",variable->name.c_str());
                    return 1;
                  }
                  status = nc_enddef(root_id);
                  if(status!=NC_NOERR){
                    CDBError("For variable %s:",variable->name.c_str());
                    ncError(__LINE__,className,"nc_enddef: ",status);return 1;}
                  status = nc_put_var(root_id,nc_var_id,variable->data);
                  if(status!=NC_NOERR){
                    CDBError("For variable %s:",variable->name.c_str());
                    ncError(__LINE__,className,"nc_put_var: ",status);return 1;}
                }
                //Free the variable data
                if(readData==true){
                  free(variable->data);variable->data=NULL;variable->setSize(0);
                }
                //CDBDebug("DONE!");
              }
              nrVarsWritten++;
            }
          }
        }
      }
      return 0;
    }
};

#endif
