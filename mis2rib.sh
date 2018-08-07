#!/bin/sh

echo converting cameras
./mis2rib camera json/cameras/shotCam.json > rib/shotCam.rib
./mis2rib camera json/cameras/beachCam.json > rib/beachCam.rib
./mis2rib camera json/cameras/birdseyeCam.json > rib/birdseyeCam.rib
./mis2rib camera json/cameras/dunesACam.json > rib/dunesACam.rib
./mis2rib camera json/cameras/grassCam.json > rib/grassCam.rib
./mis2rib camera json/cameras/palmsCam.json > rib/palmsCam.rib
./mis2rib camera json/cameras/rootsCam.json > rib/rootsCam.rib
./mis2rib camera json/cameras/shotCam.json > rib/shotCam.rib

echo converting lights
./mis2rib lights json/lights/lights.json > rib/lights.rib

echo converting elements
echo .. isBayCedarA1
./mis2rib element json/isBayCedarA1/isBayCedarA1.json > rib/isBayCedarA1.rib
echo .. isBeach
./mis2rib element json/isBeach/isBeach.json > rib/isBeach.rib
echo .. isCoastline
./mis2rib element json/isCoastline/isCoastline.json > rib/isCoastline.rib
echo .. isCoral
./mis2rib element json/isCoral/isCoral.json > rib/isCoral.rib
echo .. isDunesA
./mis2rib element json/isDunesA/isDunesA.json > rib/isDunesA.rib
echo .. isDunesB
./mis2rib element json/isDunesB/isDunesB.json > rib/isDunesB.rib
echo .. isGardeniaA
./mis2rib element json/isGardeniaA/isGardeniaA.json > rib/isGardeniaA.rib
echo .. isHibiscus
./mis2rib element json/isHibiscus/isHibiscus.json > rib/isHibiscus.rib
echo .. isHibiscusYoung
./mis2rib element json/isHibiscusYoung/isHibiscusYoung.json > rib/isHibiscusYoung.rib
echo .. isIronwoodA1
./mis2rib element json/isIronwoodA1/isIronwoodA1.json > rib/isIronwoodA1.rib
echo .. isIronwoodB
./mis2rib element json/isIronwoodB/isIronwoodB.json > rib/isIronwoodB.rib
echo .. isKava
./mis2rib element json/isKava/isKava.json > rib/isKava.rib
echo .. isLavaRocks
./mis2rib element json/isLavaRocks/isLavaRocks.json > rib/isLavaRocks.rib
echo .. isMountainA
./mis2rib element json/isMountainA/isMountainA.json > rib/isMountainA.rib
echo .. isMountainB
./mis2rib element json/isMountainB/isMountainB.json > rib/isMountainB.rib
echo .. isNaupakaA
./mis2rib element json/isNaupakaA/isNaupakaA.json > rib/isNaupakaA.rib
echo .. isPalmDead
./mis2rib element json/isPalmDead/isPalmDead.json > rib/isPalmDead.rib
echo .. isPalmRig
./mis2rib element json/isPalmRig/isPalmRig.json > rib/isPalmRig.rib
echo .. isPandanusA
./mis2rib element json/isPandanusA/isPandanusA.json > rib/isPandanusA.rib
echo .. osOcean
./mis2rib element json/osOcean/osOcean.json > rib/osOcean.rib

echo Done
