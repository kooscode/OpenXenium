
# https://help.github.com/articles/merging-an-upstream-repository-into-your-fork/
clear
echo Merging from Ryzee119 master
git checkout master
git pull https://github.com/Ryzee119/OpenXenium.git master

echo RESOLVE CONFLICTS, COMMIT AND PUSH !!!
echo ">> git add ."
echo ">> git commit -m \"Merged with latest Ryzee119 master\" "
echo ">> git push origin master"
