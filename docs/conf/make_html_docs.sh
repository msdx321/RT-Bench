#! /usr/bin/env bash
# This script will generate the documentation for all the branches of the repository in the html folder.

# create folder for the documentation
DOCS_FOLDER=$(realpath ../)"/html/branch"
mkdir -p "$DOCS_FOLDER" || exit
# we need a fresh copy of the repository to avoid conflicts while checking out branches
git clone --no-single-branch --shallow-submodules https://gitlab.com/rt-bench/rt-bench.git "$DOCS_FOLDER"/../rt-bench-tmp || exit
cd "$DOCS_FOLDER"/../rt-bench-tmp || exit
# get all branches
git fetch --all || exit
git pull --all || exit
# switch to the unstable branch (the most up to date) and initialize the submodules
git checkout dev/unstable || exit
DOCS_ONLY=1 make setup || exit
cd docs || exit
# build the documentation for the unstable branch
make html || exit
mv html/dev-unstable "$DOCS_FOLDER" || exit
# get the head.html file from the unstable branch
cp "$DOCS_FOLDER/../../conf/header.html" "$DOCS_FOLDER/../tmp_header.html" || exit
# build docs for all other branches
git for-each-ref --format='%(refname:short)' refs/heads | while read -r branch; do
	  echo "Building documentation for $branch"
		# skip the unstable branch
	  if [ "$branch" == "dev/unstable" ]; then continue; fi
    folder_name=$(echo "$branch" | sed 's/\//-/g')
		git checkout "$branch"
		rm Documentation.html
		# make sure the button to switch version is always present
		cp "$DOCS_FOLDER/../tmp_header.html" "./conf/header.html" || exit
		sed -i "s|PROJECT_NUMBER\s*=.*|PROJECT_NUMBER=$branch|" conf/Doxyfile || exit
		make html || exit
		if [ -d html/"$folder_name" ]; then
			mv html/"$folder_name" "$DOCS_FOLDER" || exit
		else
			mkdir -p "$DOCS_FOLDER/$folder_name" || exit
			mv html/* "$DOCS_FOLDER/$folder_name" || exit
		fi
		# copy the doxyversion plugin files from the unstable branch so that we can always switch between versions
		cp "$DOCS_FOLDER/dev-unstable/selectversion.js" "$DOCS_FOLDER/$folder_name" || exit
		cp "$DOCS_FOLDER/dev-unstable/dropdown.css" "$DOCS_FOLDER/$folder_name" || exit
done
cd "$DOCS_FOLDER/.." || exit
echo '<!DOCTYPE html> <html lang="en"> <head> <meta http-equiv="refresh" content="0; url=./branch/main/index.html" /> </head> </html>' > ./index.html || exit
rm -rf "$DOCS_FOLDER"/../rt-bench-tmp "$DOCS_FOLDER/../tmp_header.html"
