#include "body.h"

namespace smoothly{

void body::setBodyPosition(const std::string & uuid , const vec3 & posi){
    int cx = posi.X/32;
    int cy = posi.Z/32;

    auto it=bodies.find(uuid);
    if(it!=bodies.end()){
        bodyItem * b = it->second;
        if(b->owner == myUUID && (!myUUID.empty())){//是自己拥有的
            setCharacterChunk(uuid,cx,cy);
        }else{
            if(!chunkLoaded(cx,cy)){//在chunk外，删除物体
                releaseBody(b);
                bodies.erase(it);
                return;
            }
        }
        b->m_character.setPosition(posi);
    }
}

void body::removeBody(const std::string & uuid){
    auto it=bodies.find(uuid);
    if(it!=bodies.end()){
        releaseBody(it->second);
        bodies.erase(it);
    }
}

void body::bodyItem::updateFromWorld(){
    if(node==NULL)
        return;
    btTransform t;
    m_character.getTransform(t);
    setPositionByTransform(node,t);
}

void body::bodyItem::doAnimation(int speed, int start, int end, bool loop){
    if(node==NULL)
        return;
    node->setAnimationSpeed(speed);
    node->setLoopMode(loop);
    node->setFrameLoop(start,end);
}

body::bodyItem::bodyItem(btScalar w,btScalar h,const btVector3 & position,bool wis,bool jis):
    m_character(w,h,position,wis,jis){
}

void body::bodyAmCallback::OnAnimationEnd(irr::scene::IAnimatedMeshSceneNode *){
    parent->updateStatus(true);
}
void body::bodyItem::updateStatus(bool finish){
    auto it = parent->bodyConfig.find(id);
    if(it==parent->bodyConfig.end())
        return;
    if(it->second->aniCallback.empty())
        return;
    lua_getglobal(parent->L,it->second->aniCallback.c_str());
    if(lua_isfunction(parent->L,-1)){

        lua_pushboolean(parent->L, status.walking);
        lua_pushboolean(parent->L, status.useLeft);
        lua_pushboolean(parent->L, status.useRight);

        if(status.bodyPosture==bodyStatus::BS_BODY_STAND){
            lua_pushstring(parent->L, "stand");
        }else
        if(status.bodyPosture==bodyStatus::BS_BODY_SQUAT){
            lua_pushstring(parent->L, "squat");
        }else
        if(status.bodyPosture==bodyStatus::BS_BODY_SIT){
            lua_pushstring(parent->L, "sit");
        }else
        if(status.bodyPosture==bodyStatus::BS_BODY_RIDE){
            lua_pushstring(parent->L, "ride");
        }else
        if(status.bodyPosture==bodyStatus::BS_BODY_LIE){
            lua_pushstring(parent->L, "lie");
        }else
        if(status.bodyPosture==bodyStatus::BS_BODY_LIEP){
            lua_pushstring(parent->L, "liep");
        }else{
            lua_pushstring(parent->L, "");
        }

        if(status.handPosture==bodyStatus::BS_HAND_NONE){
            lua_pushstring(parent->L, "none");
        }else
        if(status.handPosture==bodyStatus::BS_HAND_AIM){
            lua_pushstring(parent->L, "aim");
        }else
        if(status.handPosture==bodyStatus::BS_HAND_THROW){
            lua_pushstring(parent->L, "throw");
        }else
        if(status.handPosture==bodyStatus::BS_HAND_BUILD){
            lua_pushstring(parent->L, "build");
        }else
        if(status.handPosture==bodyStatus::BS_HAND_BUILDP){
            lua_pushstring(parent->L, "buildp");
        }else
        if(status.handPosture==bodyStatus::BS_HAND_OPERATE){
            lua_pushstring(parent->L, "operate");
        }else
        if(status.handPosture==bodyStatus::BS_HAND_LIFT){
            lua_pushstring(parent->L, "lift");
        }else{
            lua_pushstring(parent->L, "");
        }

        lua_pushboolean(parent->L,finish);

        // do the call (2 arguments, 1 result)
        if (lua_pcall(parent->L, 6, 4, 0) != 0)
             printf("error running function : %s \n",lua_tostring(parent->L, -1));
        else{
            if(lua_isboolean(parent->L,-1) && lua_isinteger(parent->L,-2) && lua_isinteger(parent->L,-3) && lua_isinteger(parent->L,-4)){
                doAnimation(
                            lua_tointeger(parent->L,-4),
                            lua_tointeger(parent->L,-3),
                            lua_tointeger(parent->L,-2),
                            lua_toboolean(parent->L,-1));
            }
        }

    }
    lua_settop(parent->L , 0);
}

void body::addBody(const std::string & uuid,int id,int hp,int32_t sta_mask,const std::string & owner,const vec3 & posi,const vec3 & r,const vec3 & l){
    auto it = bodyConfig.find(id);
    if(it==bodyConfig.end())
        return;

    if(bodies.find(uuid)!=bodies.end())
        return;

    bodyConf * c = it->second;

    bodyItem * p = new bodyItem(c->width , c->height , btVector3(posi.X,posi.Y,posi.Z) , c->walkInSky , c->jumpInSky);
    p->m_character.world = dynamicsWorld;
    p->m_character.addIntoWorld();
    p->m_character.setRotation(r);
    p->lookAt = l;
    p->uuid   = uuid;
    p->owner  = owner;
    p->id     = id;
    p->hp     = hp;
    p->status = sta_mask;
    p->status_mask = sta_mask;
    p->node   = scene->addAnimatedMeshSceneNode(c->mesh);
    p->parent = this;

    //设置回调函数
    bodyAmCallback * cb = new bodyAmCallback;
    cb->parent = p;
    p->node->setAnimationEndCallback(cb);
    cb->drop();

    p->updateFromWorld();
    p->updateStatus();

    bodies[uuid] = p;
}
void body::setWearing(bodyItem * n, const std::set<int> & wearing){
    std::set<int> rm;
    for(auto it:n->wearing){
        rm.insert(it.first);
    }
    for(auto it:wearing){
        rm.erase(it);
        addWearing(n , it);
    }
    for(auto it:rm){
        removeWearing(n , it);
    }
}
void body::addWearing(bodyItem * n, int wearing){
    if(n->wearing.find(wearing)!=n->wearing.end())
        return;
    addWearingNode(n , wearing);
}
void body::removeWearing(bodyItem * n, int wearing){
    auto it = n->wearing.find(wearing);
    if(it!=n->wearing.end())
        return;
    it->second->addAnimator(scene->createDeleteAnimator(1));
    n->wearing.erase(it);
}
void body::addWearingNode(bodyItem * n, int wearing){
    auto it = wearingConfig.find(wearing);
    if(it==wearingConfig.end())
        return;
    if(it->second->attach.empty())
        return;
    auto pn = n->node->getJointNode(it->second->attach.c_str());
    auto wn = scene->addAnimatedMeshSceneNode(it->second->mesh , pn);
    n->wearing[wearing] = wn;
}
body::body(){
    loadBodyConfig();
    loadWearingConfig();
    L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dofile(L, "../script/body.lua");
}

body::~body(){
    lua_close(L);
    releaseBodyConfig();
    releaseWearingConfig();
}

void body::setPositionByTransform(irr::scene::ISceneNode * node, const btTransform & transform){
    btVector3 btPos;
    btVector3 btRot;
    irr::core::vector3df irrPos;
    irr::core::vector3df irrRot;

    btPos = transform.getOrigin();
    irrPos.set(btPos.x(), btPos.y(), btPos.z());

    const btMatrix3x3 & btM = transform.getBasis();
    btM.getEulerZYX(btRot.m_floats[2], btRot.m_floats[1], btRot.m_floats[0]);
    irrRot.X = irr::core::radToDeg(btRot.x());
    irrRot.Y = irr::core::radToDeg(btRot.y());
    irrRot.Z = irr::core::radToDeg(btRot.z());

    node->setPosition(irrPos);
    node->setRotation(irrRot);
}

int32_t body::bodyStatus::toMask()const{
    int32_t res = 0;
    if(walking)
        res |= 1;
    res |= bodyPosture;
    res |= handPosture;
    if(useLeft)
        res |= BM_HAND_LEFT;
    if(useRight)
        res |= BM_HAND_RIGHT;
    if(shotLeft)
        res |= BM_ACT_SHOT_L;
    if(shotRight)
        res |= BM_ACT_SHOT_R;
    if(throwing)
        res |= BM_ACT_THROW;
    if(chop)
        res |= BM_ACT_CHOP;
    return res;
}
void body::bodyStatus::loadMask(int32_t m){
    if(m & BM_HAND_LEFT)
        useLeft = true;
    else
        useLeft = false;

    if(m & BM_HAND_RIGHT)
        useRight = true;
    else
        useRight = false;

    handPosture = (handPosture_t)(m & BS_HAND_LIFT);
    bodyPosture = (bodyPosture_t)(m & (BS_BODY_SQUAT | BS_BODY_SIT | BS_BODY_RIDE));

    if(m & BM_WALK)
        walking = true;
    else
        walking = false;

    if(m & BM_ACT_SHOT_L)
        shotLeft = true;
    else
        shotLeft = false;

    if(m & BM_ACT_SHOT_R)
        shotRight = true;
    else
        shotRight = false;

    if(m & BM_ACT_CHOP)
        chop = true;
    else
        chop = false;

    if(m & BM_ACT_THROW)
        throwing = true;
    else
        throwing = false;
}
body::bodyStatus::bodyStatus(){
    bodyPosture = BS_BODY_STAND;
    handPosture = BS_HAND_NONE;
    useLeft  = false;
    useRight = false;
    walking  = false;
    shotLeft = false;
    shotRight= false;
    throwing = false;
    chop     = false;
}
body::bodyStatus::bodyStatus(const bodyStatus & i){
    handPosture = i.handPosture;
    bodyPosture = i.bodyPosture;
    useLeft     = i.useLeft;
    useRight    = i.useRight;
    walking     = i.walking;
    shotLeft    = i.shotLeft;
    shotRight   = i.shotRight;
    throwing    = i.throwing;
    chop        = i.chop;
}
body::bodyStatus::bodyStatus(int32_t m){
    loadMask(m);
}
const body::bodyStatus & body::bodyStatus::operator=(const bodyStatus & i){
    handPosture = i.handPosture;
    bodyPosture = i.bodyPosture;
    useLeft     = i.useLeft;
    useRight    = i.useRight;
    walking     = i.walking;
    shotLeft    = i.shotLeft;
    shotRight   = i.shotRight;
    throwing    = i.throwing;
    chop        = i.chop;
    return *this;
}
const body::bodyStatus & body::bodyStatus::operator=(int32_t m){
    loadMask(m);
    return *this;
}

void body::releaseBody(bodyItem * b){
    b->m_character.removeFromWorld();
    b->node->addAnimator(scene->createDeleteAnimator(1));
    delete b;
}

}