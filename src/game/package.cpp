#include "package.h"
#include <QFile>

namespace smoothly{

package::package(){
    packageRoot = scene->addEmptySceneNode();
    selectedPackageSceneNode = NULL;
    autoPickup = false;

    printf("[status]get package config\n" );
    QFile file("../config/package.json");
    if(!file.open(QFile::ReadOnly)){
        printf("[error]fail to read ../config/package.json\n" );
        return;
    }
    QByteArray allData = file.readAll();
    file.close();
    auto str = allData.toStdString();
    cJSON * json=cJSON_Parse(str.c_str());
    if(json){
        if(json->type==cJSON_Array){
            cJSON *c=json->child;
            while (c){
                if(c->type==cJSON_Object){
                    auto idnode = cJSON_GetObjectItem(c,"id");
                    if(idnode && idnode->type==cJSON_Number){
                        int id = idnode->valueint;
                        if(package_configs.find(id)==package_configs.end()){
                            auto meshnode = cJSON_GetObjectItem(c,"mesh");
                            if(meshnode && meshnode->type==cJSON_String){
                                auto mesh = scene->getMesh(meshnode->valuestring);
                                if(mesh){
                                    auto conf = new package_config_t;
                                    conf->id = id;
                                    conf->mesh = mesh;
                                    package_configs[id] = conf;
                                }
                            }
                        }
                    }
                }
                c=c->next;
            }
        }else{
            printf("[error]root in ../config/package.json is not Array!\n" );
        }
        cJSON_Delete(json);
    }else{
        printf("[error]fail to load json\n" );
    }
    packageShader_normal = driver->getGPUProgrammingServices()->addHighLevelShaderMaterialFromFiles(
                          "../shader/package.vs.glsl", "main", irr::video::EVST_VS_1_1,
                          "../shader/package.ps.glsl", "main", irr::video::EPST_PS_1_1,
                          &defaultCallback);
    packageShader_select = driver->getGPUProgrammingServices()->addHighLevelShaderMaterialFromFiles(
                          "../shader/package.vs.glsl", "main", irr::video::EVST_VS_1_1,
                          "../shader/package_selected.ps.glsl", "main", irr::video::EPST_PS_1_1,
                          &defaultCallback);
}

package::~package(){
    for(auto it:package_configs){
        delete it.second;//mesh会被自动释放
    }
}

void package::addPackage(int id ,int x , int y , const vec3 & posi, const std::string & uuid){
    if(packages.find(uuid)==packages.end()){//没被创建过
        package_t pack;
        auto confid = package_configs.find(id);
        if(confid!=package_configs.end()){//模型存在
            package_config_t * conf = confid->second;
            if(conf){
                pack.conf = conf;
                pack.uuid = uuid;
                pack.cx   = x;
                pack.cy   = y;
                pack.node = scene->addMeshSceneNode(conf->mesh,packageRoot);
                pack.node->setMaterialType((video::E_MATERIAL_TYPE)packageShader_normal);
                //pack.node->setMaterialTexture(1,shadowMapTexture);
                vec3 tmpp = posi;
                auto hei = getRealHight(tmpp.X,tmpp.Z);
                if(tmpp.Y<hei)
                    tmpp.Y = hei;
                else{
                    vec3 hto = tmpp;
                    hto.Y -= 32;
                    fetchByRay(posi,hto,[&](const vec3 & p,bodyInfo * ){
                        tmpp = p;
                    });
                }
                pack.node->setPosition(tmpp);
                pack.node->setName(uuid.c_str());
                packages[uuid] = pack;
            }
        }
    }
}

void package::removePackage(const std::string & uuid){
    auto pkit = packages.find(uuid);
    if(pkit!=packages.end()){
        package_t & pack = pkit->second;
        if(pack.node){
            if(pack.node==selectedPackageSceneNode)
                selectedPackageSceneNode = NULL;
            pack.node->remove();
        }
        packages.erase(pkit);
    }
}

void package::pickupPackage(){
    if(selectedPackageSceneNode){
        auto packit = packages.find(selectedPackageSceneNode->getName());
        if(packit!=packages.end()){
            pickupPackageToBag(packit->second.cx,packit->second.cy,packit->second.uuid);
        }
    }
}

void package::msg_package_add(int32_t x, int32_t y, const char * uuid, const char * text){
    auto json = cJSON_Parse(text);

    if(json){

        cJSON *c=json->child;
        vec3 posi;
        int skin = 0;
        while(c){
            if(c->type==cJSON_Number){
                if(strcmp(c->string,"x")==0){
                    posi.X = c->valuedouble;
                }else if(strcmp(c->string,"y")==0){
                    posi.Y = c->valuedouble;
                }else if(strcmp(c->string,"z")==0){
                    posi.Z = c->valuedouble;
                }else if(strcmp(c->string,"skin")==0){
                    skin = c->valuedouble;
                }
            }
            c = c->next;
        }

        addPackage(skin,x,y,posi,uuid);

        cJSON_Delete(json);
    }
}

void package::msg_package_remove(int32_t, int32_t, const char * uuid){
    removePackage(uuid);
}

void package::loop(){
    terrainDispather::loop();
    if(selectedPackageSceneNode){
        //selectedPackageSceneNode->setMaterialFlag(irr::video::EMF_LIGHTING, true );
        selectedPackageSceneNode->setMaterialType((video::E_MATERIAL_TYPE)packageShader_normal);
        selectedPackageSceneNode = NULL;
    }
    irr::core::line3df ray;
    ray.start   = camera->getPosition();
    ray.end     = camera->getTarget();
    auto p = collisionManager->getSceneNodeFromRayBB(ray,0,false,packageRoot);
    if(p && (p->getPosition()-ray.start).getLengthSQ()<8){
        //p->setMaterialFlag(irr::video::EMF_LIGHTING, false);
        p->setMaterialType((video::E_MATERIAL_TYPE)packageShader_select);
        selectedPackageSceneNode = p;
        if(autoPickup){
            auto packit = packages.find(selectedPackageSceneNode->getName());
            if(packit!=packages.end()){
                pickupPackageToBag(packit->second.cx,packit->second.cy,packit->second.uuid);
            }
        }
    }
}

}
