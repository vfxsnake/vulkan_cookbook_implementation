# Chapter 8: Graphics Rendering Pipeline

*From: Vulkan 3D Graphics Rendering Cookbook, 2nd Edition*

---

## Overview

In this chapter, we will learn how to build a hierarchical scene representation and set up a rendering 
pipeline that combines the rendering of geometry and materials, as discussed in earlier chapters. 
Instead of implementing a naïve object-oriented scene graph, where each node is an object stored 
on the heap, we will explore a data-oriented design approach. This will simplify the memory layout 
of our scene and greatly improve the performance of scene graph manipulations. This approach also 
serves as an introduction to data-oriented design principles and how to apply them in practice. The 
scene graph and material representations we cover are compatible with glTF 2.0. We will also look 
at how to organize the rendering process for complex scenes with multiple materials. We will cover 
the following recipes:
- How not to do a scene graph
- Using data-oriented design for a scene graph
- Loading and saving a scene graph
- Implementing transformation trees
- Implementing a material system
- Implementing automatic material conversion
- Using descriptor indexing and arrays of textures in Vulkan
- Implementing indirect rendering with Vulkan
- Putting it all together into a scene editing application
- Deleting nodes and merging scene graphs
- Rendering large scenes

## How not to do a scene graph

Numerous hobby 3D engines take a straightforward and naïve class-based approach to implementing 
a scene graph. It is always tempting to define a structure like the one in the following code; please 
do not do that though:
struct SceneNode {
 SceneNode* parent;


 vector<SceneNode*> children;
 mat4 localTransform;
 mat4 globalTransform;
 Mesh* mesh;
 Material* material;
 void render();
};
On top of this structure, one might define various recursive traversal methods, such as the dreaded 
render() function. For example, let’s say we have a root object:
SceneNode* root;
Then, rendering the scene graph could be as simple as:
root->render();
In this approach, the render() method performs several tasks. First, it calculates the global transform for the current node based on the local transform, which is relative to the parent node. Then, 
depending on the rendering API being used, it sends the mesh geometry and material information 
to the rendering pipeline. Finally, it makes a recursive call to render all the child nodes. Things can 
get tricky when you need to handle transparent nodes, and your results may vary:
void SceneNode::render() {
 globalTransform =
 (parent ? parent->globalTransform : mat4(1)) * localTransform;
 apiSpecificRenderCalls();
 for (auto& c: this->children)
 c->render();
}
While this is a simple and “canonical” object-oriented approach, it comes with several significant 
drawbacks:
- Non-locality of data due to the use of pointers, unless a custom memory allocator is implemented.
- Performance issues arise due to explicit recursion, which can become especially problematic 
with deep recursion.
- Risk of memory leaks and crashes when using raw pointers, or a slowdown from atomic operations if smart pointers are used instead.
- Challenges with circular references, requiring the use of weak pointers or other tricks to avoid 
memory leaks when using smart pointers.


- Recursive loading and saving of the structure is complicated, slow, and prone to errors. We’ve 
also seen the visitor pattern used in this context, which only adds to the complexity without 
solving any performance issues.
- Difficulty implementing extensions, as it requires continually adding more and more fields 
to the SceneNode structure.
As the 3D engine grows and the requirements for the scene graph increase, more fields, arrays, callbacks, and pointers need to be added and managed within the SceneNode structure. This makes the 
approach (besides being slow) fragile and difficult to maintain over time.
Let’s take a step back and reconsider how we can preserve the relative structure of the scene without 
relying on large, monolithic classes filled with heavy dynamic containers.

## Using data-oriented design for a scene graph

To represent complex nested visual objects like robotic arms, planetary systems, or intricately branched 
animated trees, one effective approach is to break the object into smaller parts and track the hierarchical relationships between them. This is known as a scene graph, which is a directed graph showing 
the parent-child relationships among various objects in a scene. We avoid using the term “acyclic 
graph” because, for convenience, it’s sometimes useful to have controlled circular references between 
nodes. Many 3D graphics tutorials for hobbyists take a straightforward but less optimal approach, as 
we discussed in the previous recipe, How not to do a scene graph. Now, let’s go deeper into the rabbit 
hole and explore how to use data-oriented design to build a more efficient scene graph.
In this recipe, we will learn how to get started with designing a reasonably efficient scene graph, with 
a focus on fixed hierarchies. In Chapter 11, we will take a closer look at this topic, discussing how to 
handle runtime topology changes and other scene graph operations.

### Getting ready

The source code for this recipe is a part of the scene rendering apps Chapter08/02_SceneGraph and 
Chapter08/03_LargeScene. In the previous edition of our book, we included this code in a separate 
scene preprocessing tool that had to be run manually before the demo apps could use the converted 
data. This method turned out to be cumbersome and confusing for many readers. This time, all scene 
conversion and preprocessing tasks will be handled automatically. Let’s see how to do it.

### How to do it...

Let’s break down and refactor the SceneNode struct from the previous recipe How not to do a scene graph, 
step by step. It makes sense to store a linear array of individual nodes and replace all the “external” 
pointers, like Mesh* and Material*, with appropriately-sized integer handles that simply act as indices into corresponding arrays. We will keep the array of child nodes and references to parent nodes 
separate. This approach not only significantly improves cache locality but also reduces memory usage, 
as pointers are 64-bit, while indices can be just 32-bit or, in many scenarios, even 16-bit.


The local and global transforms are also stored in separate arrays, which can, by the way, be easily 
mapped to a GPU buffer without conversion, making it directly accessible from GLSL shaders. Let’s 
look at the implementation:
1. We have a new simplified scene node declaration and our new scene is a composite of arrays:
struct SceneNode {
 int mesh_;
 int material_;
};
struct Scene {
 vector<SceneNode> nodes_;
 vector<mat4> local_;
 vector<mat4> global_;
};
2. One question remains: how do we store the hierarchy where each node can have multiple 
children? A well-known solution is the Left Child, Right Sibling tree representation. Since a 
scene graph is really a tree, at least in theory, without any optimization-related circular references, we can convert any tree with more than two children into a binary tree by “tilting” its 
branches, as illustrated in the following figure:
Figure 8.1: Tree representations
More information
For more information on the “Left Child, Right Sibling” representation, you can 
check out the additional reading available on Wikipedia at https://en.wikipedia.
org/wiki/Left-child_right-sibling_binary_tree.


1. In this figure 8.1, the left image depicts a standard tree where each node can have a variable 
number of children, while the right image illustrates a new structure that stores only a single 
reference to the first child and another reference to the next “sibling.” Here, “sibling node” 
refers to a child node of the same parent. This transformation eliminates the need to store 
a std::vector in each scene node. If we then “tilt” the right image, we arrive at a familiar 
binary tree structure, where solid left-pointing arrows represent the “First Child” reference 
and dashed right-pointing arrows indicate the “Next Sibling” reference:
Figure 8.2: A tilted tree
2. Let’s incorporate indices into the SceneNode structure to reflect this storage schema. In addition 
to the mesh and material indices for each node, we will include a reference to the parent, an 
index for the first child (or a negative value if there are no child nodes), and an index for the 
next sibling scene node:
struct SceneNode {
 int mesh_, material_;
 int parent_;
 int firstChild_;
 int rightSibling_;
};
What we have now is a compact constant-sized object that is plain old data. While the tree traversal and modification routines may seem unconventional, they are essentially just iterations 
over a linked list. It’s important to note a significant drawback: random access to a child node 
is now slower on average since we must traverse each node in the list. However, this isn’t a 
major issue for our purposes, as we typically traverse either all the children or none of them.


3. Before we move on to the implementation, let’s make another unconventional transformation 
to the new SceneNode structure. It currently contains indices for the mesh and material, along 
with hierarchical information, but the local and global transformations are stored separately. 
This indicates that we can declare the following structure shared/Scene/Scene.h to manage 
our scene hierarchy:
struct Hierarchy {
 int parent = -1;
 int firstChild = -1;
 int nextSibling = -1;
 int lastSibling = -1;
 int level = 0;
};
The lastSibling member field is a way to speed up the process of adding new child nodes to 
a parent. We’ve renamed “left” to “first” and “right” to “next” because relative positions of the 
tree nodes are not relevant in this context. The level field stores a cached depth of the node 
from the root of the scene graph. The root node has a level of zero, and each child node’s level 
is one greater than that of its parent.
4. Now, we can declare a new Scene structure in shared/Scene/Scene.h with logical “compartments,” which we will later refer to as components:
struct Scene {
 vector<mat4> localTransforms;
 vector<mat4> globalTransforms;
 vector<Hierarchy> hierarchy;
 // Meshes for scene nodes (Node -> Mesh)
 unordered_map<uint32_t, uint32_t> meshForNode;
 // Materials for scene nodes (Node -> Material)
 unordered_map<uint32_t, uint32_t> materialForNode;
Note
Additionally, the Mesh and Material objects for each node can be stored in separate arrays. However, if not all nodes have a mesh or material, we can utilize a 
sparse representation, such as a hash table, to map nodes to their corresponding 
meshes and materials. The absence of such mapping simply indicates that a scene 
node is used solely for storing transformations or hierarchical relationships. While 
hash tables are not as “linear” as arrays, they can easily be converted to and from 
arrays of {key, value} pairs of some sort. We use std::unordered_map for the 
sake of simplicity. Try choosing a faster hash table for your production code. If 
the majority of your scene nodes contain meshes and materials, you might want 
to prefer using dense storage data structures, such as arrays, right from the get-go.


5. The following components are not strictly necessary but they help a lot while debugging scene 
graph manipulation routines or while implementing an interactive scene editor, where the 
ability to see some human-readable node identifiers is crucial:
 // Node name component: (Node -> name)
 unordered_map<uint32_t, uint32_t> nameForNode;
 // Collection of scene node names
 vector<std::string> nodeNames;
 // Collection of debug material names
 vector<std::string> materialNames;
};
One thing that’s missing is the SceneNode structure itself, which is now represented by integer 
indices in the arrays of the Scene container. It might feel quite amusing and unconventional 
for someone with an object-oriented mindset to discuss SceneNode without actually having 
or needing a scene node class.
The routine for converting Assimp’s aiScene into our scene format is implemented in the shared 
header file Chapter08/SceneUtils.h. This process uses a top-down recursive traversal, creating implicit scene node objects within the Scene container. Let’s walk through the steps needed to traverse 
a scene stored in this format.
1. The traversal starts from some aiNode and a given parent node ID, passed as a parameter. A 
new node identifier is returned by the addNode() routine described below. If aiNode contains 
a name, we store it in the Scene::nodeNames array:
void traverse(const aiScene* sourceScene,
 Scene& scene, aiNode* node,
 int parent, int atLevel)
{
 const int newNodeID = addNode(scene, parent, atLevel);
 if (node->mName.C_Str()) {
 uint32_t stringID = (uint32_t)scene.nodeNames.size();
 scene.nodeNames.push_back(std::string(node->mName.C_Str()));
 scene.nameForNode[newNodeID] = stringID;
 }
2. If the aiNode object has meshes attached to it, we create a subnode for each of those meshes. 
To make debugging easier, we also add the name of each new mesh subnode:
 for (size_t i = 0; i < node->mNumMeshes; i++) {
 const int newSubNodeID = addNode(scene, newNode, atLevel+1);
 const uint32_t stringID = (uint32_t)scene.nodeNames.size();
 scene.nodeNames.push_back(
 std::string(node->mName.C_Str()) + "_Mesh_" + std::to_string(i));
 scene.nameForNode[newSubNodeID] = stringID;


3. Each of the meshes is assigned to the newly created subnode. Assimp guarantees a mesh has 
its material assigned, so we assign that material to our node. Since we use subnodes only for 
attaching meshes, we set local and global transformations to identity:
 const int mesh = (int)node->mMeshes[i];
 scene.meshForNode[newSubNodeID] = mesh;
 scene.materialForNode[newSubNodeID] =
 sourceScene->mMeshes[mesh]->mMaterialIndex;
 scene.globalTransform[newSubNode] = mat4(1.0f);
 scene.localTransform[newSubNode] = mat4(1.0f);
 }
4. The global transformation is set to identity at the beginning of node conversion. It will be 
recalculated at the first frame or if the node is marked as changed. See the Implementing transformation trees recipe in this chapter for the implementation details. The local transformation 
is fetched from aiNode and converted into a glm::mat4 object:
 scene.globalTransform[newNode] = mat4(1.0f);
 scene.localTransform[newNode] = toMat4(N->mTransformation);
5. At the end, we recursively traverse the children of this aiNode object:
 for (unsigned int n = 0; n < N->mNumChildren; n++)
 traverse(sourceScene, scene,
 N->mChildren[n], newNode, atLevel+1);
}
The most complex part of this code that deals with the Scene data structure is the addNode() routine, 
which allocates a new scene node and adds it to the scene hierarchy. Let’s take a look at how to implement it in shared/Scene/Scene.cpp.
1. First, the addition process obtains a new node identifier, which corresponds to the current size 
of the hierarchy array. New identity transformations are then added to the local and global 
transform arrays. The hierarchy for the newly added node consists solely of the reference to 
its parent:
int addNode(Scene& scene, int parent, int level) {
 const int node = (int)scene.hierarchy.size();
 scene.localTransform.push_back(mat4(1.0f));
 scene.globalTransform.push_back(mat4(1.0f));
 scene.hierarchy.push_back({ .parent = parent });


2. If we have a parent node, we update its first child reference and, potentially, the next sibling 
reference of another node. If the parent node has no children, we simply set its firstChild
field. Otherwise, we need to traverse the siblings of this child to find the appropriate place to 
add the next sibling:
 if (parent > -1) {
 const int s = scene.hierarchy[parent].firstChild;
 if (s == -1) {
 scene.hierarchy[parent].firstChild = node;
 scene.hierarchy[node].lastSibling = node;
 } else {
 int dest = scene.hierarchy[s].lastSibling;
 if (dest <= -1) {
 for (dest = s;
 scene.hierarchy[dest].nextSibling !=-1;
 dest = scene.hierarchy[dest].nextSibling);
 }
 scene.hierarchy[dest].nextSibling = node;
 scene.hierarchy[s].lastSibling = node;
 }
 }
After the for loop, we set our new node as the next sibling of the last child. It’s worth noting 
that this linear traversal of the siblings isn’t necessary if we store the index of the last added 
child node. Later, in the Implementing transformation trees recipe, we’ll demonstrate how to 
modify addNode() to eliminate the loop.
3. The level of this node is stored for correct global transformation updating. To keep the structure 
valid, we store negative indices for the newly added node:
 scene.hierarchy[node].level = level;
 scene.hierarchy[node].nextSibling = -1;
 scene.hierarchy[node].firstChild = -1;
 return node;
}
We use this traverse() routine to load mesh data with Assimp in our sample applications using a 
helper function, loadMeshFile().
There’s more...
Data-oriented design (DOD) is a broad field, and we’ve only covered a few techniques here. We recommend checking out the online book Data-Oriented Design by Richard Fabian to become more familiar 
with additional DOD concepts: https://www.dataorienteddesign.com/dodbook.


The demo application Chapter08/02_SceneGraph implements some basic scene graph editing capabilities using ImGui, which can help you get started with integrating scene graphs into your productivity 
tools and writing an editor for your 3D applications. The following renderSceneTreeUI() recursive 
function from Chapter08/02_SceneGraph/src/main.cpp is responsible for the rendering of the scene 
graph tree hierarchy in the UI and selecting a node for editing:
int renderSceneTreeUI(
 const Scene& scene, int node, int selectedNode)
{
 const std::string name = getNodeName(scene, node);
 const std::string label = name.empty() ?
 (std::string("Node") + std::to_string(node)) : name;
 const bool isLeaf = scene.hierarchy[node].firstChild<0;
 ImGuiTreeNodeFlags flags = isLeaf ?
 ImGuiTreeNodeFlags_Leaf |
 ImGuiTreeNodeFlags_Bullet : 0;
 if (node == selectedNode)
 flags |= ImGuiTreeNodeFlags_Selected;
 ImVec4 color = isLeaf ? ImVec4(0, 1, 0, 1) : ImVec4(1, 1, 1, 1);
 ImGui::PushStyleColor(ImGuiCol_Text, color);
 const bool isOpened = ImGui::TreeNodeEx(
 &scene.hierarchy[node], flags, "%s", label.c_str());
 ImGui::PopStyleColor();
 ImGui::PushID(node);
 if (ImGui::IsItemHovered() && isLeaf)
 selectedNode = node;
 if (isOpened) {
 for (int ch = scene.hierarchy[node].firstChild;
 ch != -1;
 ch = scene.hierarchy[ch].nextSibling) {
 if (int subNode =
 renderSceneTreeUI(scene, ch, selectedNode);
 subNode > -1) {
 selectedNode = subNode;
 }
 }
 ImGui::TreePop();
 }
 ImGui::PopID();
 return selectedNode;
}


This function can be used as a basis for building various editing functionality for nodes, materials, 
and other scene graph content. Take a look at editNodeUI() and editMaterialUI() for some more 
examples.

## Loading and saving a scene graph

To quote Frederick Brooks, “Show me your data structures, and I don’t need to see your code.” By now, 
you should have a pretty good idea of how to implement basic operations on the scene graph. However, 
the rest of this chapter will walk you through all the necessary routines in detail. In this section, we’ll 
provide an overview of how to load and save our scene graph structure.

### Getting ready

Make sure you read the previous recipe, Using data-oriented design for a scene graph, before proceeding 
any further.

### How to do it...

The loading process consists of a series of fread() calls, followed by two loadMap() calls, which are 
handled within the loadScene() helper function located in shared/Scene/Scene.cpp. As with other 
examples in this book, we’ve left out error handling in the text, but the accompanying source code 
includes the necessary checks.
1. After opening the file, we read the number of scene nodes stored. Then, we resize the data 
containers to match:
void loadScene(const char* fileName, Scene& scene) {
 FILE* f = fopen(fileName, "rb");
 uint32_t sz = 0;
 fread(&sz, sizeof(sz), 1, f);
 scene.hierarchy.resize(sz);
 scene.globalTransform.resize(sz);
 scene.localTransform.resize(sz);
2. A series of fread() calls is sufficient to read the transformations and hierarchical data for all 
the scene nodes:
 fread(scene.localTransform.data(), sizeof(glm::mat4), sz, f);
 fread(scene.globalTransform.data(), sizeof(glm::mat4), sz, f);
 fread(scene.hierarchy.data(), sizeof(Hierarchy), sz, f);
3. The node-to-material and node-to-mesh mappings are loaded using the loadMap() helper 
function. We will look into it in a moment:
 loadMap(f, scene.materialForNode);
 loadMap(f, scene.meshForNode);


4. If any data remains, we proceed to read the scene node names and material names:
 if (!feof(f)) {
 loadMap(f, scene.nameForNode);
 loadStringList(f, scene.nodeNames);
 loadStringList(f, scene.materialNames);
 }
 fclose(f);
}
Saving the scene data is essentially the reverse of the loadScene() routine. Let’s take a look at how 
it’s implemented:
1. At the start of the file, we write the number of scene nodes:
void saveScene(
 const char* fileName, const Scene& scene)
{
 FILE* f = fopen(fileName, "wb");
 const uint32_t sz = (uint32_t)scene.hierarchy.size();
 fwrite(&sz, sizeof(sz), 1, f);
2. Three fwrite() calls save the local and global transformations, followed by the hierarchical 
information:
 fwrite(scene.localTransform.data(), sizeof(glm::mat4), sz, f);
 fwrite(scene.globalTransform.data(), sizeof(glm::mat4), sz, f);
 fwrite(scene.hierarchy.data(), sizeof(Hierarchy), sz, f);
3. Two saveMap() calls store the node-to-material and node-to-mesh mappings:
 saveMap(f, scene.materialForNode);
 saveMap(f, scene.meshForNode);
4. If the scene node and material names are not empty, we also save these mappings:
 if (!scene.nodeNames.empty() &&
 !scene.nameForNode.empty()) {
 saveMap(f, scene.nameForNode);
 saveStringList(f, scene.nodeNames);
 saveStringList(f, scene.materialNames);
 }
 fclose(f);
}
Let’s briefly describe the helper routines loadMap() and saveMap(), which handle loading and saving 
our std::unordered_map containers, respectively. Loading a std::unordered_map is done in three 
steps:


1. First, we read the number of integer values from the file. Each of our {key, value} pairs 
consists of two unsigned integers, which are stored sequentially:
void loadMap(
 FILE* f, unordered_map<uint32_t, uint32_t>& map)
{
 vector<uint32_t> ms;
 uint32_t sz = 0;
 fread(&sz, 1, sizeof(sz), f);
2. Then, all the key and value pairs are loaded with a single fread() call:
 ms.resize(sz);
 fread(ms.data(), sizeof(uint32_t), sz, f);
3. Finally, all the pairs from the array are inserted into a hash table:
 for (size_t i = 0; i < (sz / 2); i++)
 map[ms[i * 2 + 0]] = ms[i * 2 + 1];
}
The saving routine for std::unordered_map is implemented by reversing the loadMap() process, line 
by line:
1. First, we allocate a temporary vector to hold the {key, value} pairs:
void saveMap(FILE* f,
 const std::unordered_map<uint32_t, uint32_t>& map)
{
 vector<uint32_t> ms;
 ms.reserve(map.size() * 2);
2. All the values from std::unordered_map are copied into this vector:
 for (const auto& m : map) {
 ms.push_back(m.first);
 ms.push_back(m.second);
 }
3. The number of stored integer values is written to the file, followed by the actual integer values 
that represent our {key, value} pairs. This is done using fwrite() calls:
 const uint32_t sz = static_cast<uint32_t>(ms.size());
 fwrite(&sz, sizeof(sz), 1, f);
 fwrite(ms.data(), sizeof(uint32_t), ms.size(), f);
}


There’s more...
Changes to the topology of nodes in our scene graph present a (solvable) management problem. The 
relevant source code is discussed in the recipe Deleting nodes and merging scene graphs in Chapter 11. 
We just need to keep all the mesh geometries in a single large GPU buffer, and we will explain how to 
implement this later in the chapter.
The material conversion routines will be implemented in the Implementing a material system recipe 
later in this chapter, along with the complete scene loading and saving code that includes the scene 
mesh geometry. Now, let’s continue exploring our scene graph management code.

## Implementing transformation trees

The typical application of a scene graph is to represent spatial relationships. For rendering, we need 
to compute a global 3D affine transformation for each node in the scene graph. This recipe explains 
how to calculate global transformations from local ones, avoiding unnecessary computations.

### Getting ready

Using the previously defined Scene structure, we demonstrate how to recalculate global transformations. It’s helpful to review the Using data-oriented design for a scene graph recipe before continuing. 
To begin, let’s revisit a risky but tempting idea we had earlier – a recursive global transformation 
calculator within the non-existent SceneNode::render() method:
SceneNode::render() {
 mat4 parentTransform = parent ? parent->globalTransform : mat4(1);
 this->globalTransform = parentTransform * localTransform;
 // ... rendering and recursion
}
This approach results in data being scattered across memory, causing cache thrashing and severely 
reducing performance.
It is always better to separate operations like rendering, scene traversal, and transform calculation, 
and to store related data close together in memory. Additionally, processing similar tasks in large 
batches helps. This separation becomes even more critical as the number of nodes grows.
We’ve already seen how to render a large number of static meshes in a single GPU draw call by combining indirect rendering with programmable vertex pulling. Here, we will focus on how to minimize 
global transformation recalculations.
More information
For more information about caches and performance, check out the CppCon 2014 presentation Data-Oriented Design and C++ by Mike Acton.


### How to do it...

It is always best to avoid unnecessary calculations. For global transformations of scene nodes, we need 
a way to flag specific nodes whose transforms have changed in the current frame. Since changes to 
a node can affect its children, we also need to mark those children as changed. To handle this, let’s 
introduce a data structure to track “dirty” nodes that have been updated since the last frame. The full 
source code is located in shared/Scene/Scene.cpp.
1. In the Scene structure, we should declare an array of std::vectors called changedAtThisFrame
to quickly track any node that has changed at a specific depth level in the scene graph. This is 
why we needed to store each node’s depth level value:
struct Scene {
 // …
 vector<int> changedAtThisFrame[MAX_NODE_LEVEL];
};
2. The markAsChanged() function starts with a given node ID and recursively traverses all its 
child nodes, adding them to the changedAtThisFrame array. The process begins by marking 
the node itself as changed:
void markAsChanged(Scene& scene, int node) {
 const int level = scene.hierarchy_[node].level;
 scene.changedAtThisFrame_[level].push_back(node);
3. Next, we move to the first child and proceed to the next sibling, continuing to descend the 
hierarchy:
 for (int s = scene.hierarchy[node].firstChild;
 s != -1;
 s = scene.hierarchy[s].nextSibling) {
 markAsChanged(scene, s);
 }
}
Read more
A pedantic reader might point out that with a slightly more sophisticated dirty 
flag tracking, we could avoid making markAsChanged() recursive and instead 
implement it as a pure O(1) call. This is correct, and those interested in that approach are encouraged to check out the presentation Handling Massive Transform 
Updates in a SceneGraph by Markus Tavenrath (https://x.com/CorporateShark/
status/1840797491140444290) and his TransformTree data structure. However, 
we will stick to a simpler method here for the sake of easier teaching.


To recalculate all the global transformations for the changed nodes, we implement the following 
recalculateGlobalTransforms() function. If no local transformations have been updated and the 
scene is essentially static, no processing will occur.
1. We begin with the list of changed scene nodes at depth level 0, assuming there is only one root 
node. The root node is unique because it has no parent, and its global transform is identical to 
its local transform. This design helps us avoid unnecessary branching by not requiring a parent 
check for all other nodes. After the update, we clear the list of changed nodes depth at level 0:
void recalculateGlobalTransforms(Scene& scene) {
 if (!scene.changedAtThisFrame_[0].empty()) {
 const int c = scene.changedAtThisFrame_[0][0];
 scene.globalTransform_[c] = scene.localTransform_[c];
 scene.changedAtThisFrame_[0].clear();
 }
2. For all the higher depth levels, we can be certain that there are parents, so the loop runs in a 
linear fashion without any conditions inside. We begin at the depth level 1 since the root level 
has already been processed. The exit condition for the loop is when the current level’s list is 
empty. Additionally, we ensure we don’t descend deeper than MAX_NODE_LEVEL:
 for (int i = 1 ; i < MAX_NODE_LEVEL &&
 (!scene.changedAtThisFrame[i].empty()); i++ )
 {
3. We iterate through all the changed nodes at the current depth level. For each node we process, 
we retrieve the parent global transform and multiply it by the local node transform:
 for (int c : scene.changedAtThisFrame[i]) {
 int p = scene.hierarchy[c].parent;
 scene.globalTransform[c] =
 scene.globalTransform[p] *
 scene.localTransform[c];
 }
4. At the end of the node iteration, we should clear the list of changed nodes for this depth level:
 scene.changedAtThisFrame[i].clear();
 }
}
The core of this approach is that we avoid recalculating any global transformations multiple times. 
By starting from the root layer of the scene graph tree, all the changed layers below the root obtain a 
valid global transformation from their parents.


There’s more...
As an advanced exercise, you can offload the computation of changed node transformations to the 
GPU. This is relatively straightforward to implement, given that all the data is stored in arrays and we 
have compute shaders and buffer management code already in place. Those interested in pursuing 
a GPU version are encouraged to check out the presentation Handling Massive Transform Updates in a 
SceneGraph by Markus Tavenrath.

## Implementing a material system

Chapter 6 provided an overview of the PBR shading model and included all the necessary GLSL shaders 
for rendering a single 3D object with multiple textures. In this chapter, we focus on how to organize 
scene rendering with multiple objects, each having different materials and properties. Our scene 
material system uses the same GLTFMaterialDataGPU structure from shared/UtilsGLTF.h and is compatible with our glTF 2.0 rendering code. However, to simplify the understanding of the code flow, we 
will focus on the scene representation and won’t delve into parsing all the material properties again.

### Getting ready

In the previous chapters, we focused on rendering individual objects and applying a PBR shading 
model to them. In the recipe Using data-oriented design for a scene graph, we learned the overall structure for organizing a scene and used opaque integers as material handles. Here, we will define a data 
structure for storing material parameters for the entire scene and demonstrate how this structure 
can be utilized in GLSL shaders. The process of converting material parameters from those loaded by 
Assimp will be explained later in this chapter in the recipe Implementing automatic material conversion.
Be sure to review the recipe Organizing mesh data storage in Chapter 5 to refresh your understanding 
of how scene geometry data is organized.

### How to do it...

We need data structures to represent our materials in both CPU memory, for quick loading and saving 
to a file, and in a GPU buffer. We already have GLTFMaterialDataGPU for the latter purpose. Now, let’s 
introduce a similar structure for the CPU.
1. One reason for using a custom data structure is to allow for loading and storing material data 
without the need to parse complex glTF files. Another reason is to include additional data for 
our rendering needs. Let’s introduce a set of convenience flags for our materials:
enum MaterialFlags {
Note 
Depending on how often local transformations are updated, it may be more efficient to 
forgo the list of recently updated nodes and simply perform a full update every time. Be 
sure to profile your actual code before making a decision.


 sMaterialFlags_CastShadow = 0x1,
 sMaterialFlags_ReceiveShadow = 0x2,
 sMaterialFlags_Transparent = 0x4,
};
2. The Material struct includes both the numeric values that define the basic lighting properties 
of the material and a set of texture indices. The first two fields store the emissive and diffuse 
colors for our simple shading model:
struct Material {
 vec4 emissiveFactor = vec4(0, 0, 0, 0);
 vec4 baseColorFactor = vec4(1, 1, 1, 1);
 float roughness = 1.0f;
 float transparencyFactor = 1.0f;
 float alphaTest = 0.0f;
 float metallicFactor = 0.0f;
3. The textures are stored as indices in a MeshData::textureFiles array, which we will discuss 
in the next step. This approach is essential for deduplicating textures, as different materials 
may utilize the same texture that we want to reuse. It is important to note that the indices 
here are signed integers, where a value of -1 indicates the absence of a texture. In contrast, 
the GPU version of this structure uses unsigned values and dummy textures. To customize our 
rendering pipeline, we might want to use flags that vary from material to material or from 
object to object. In the demos for this book, we don’t require this level of flexibility, as we render all objects with a single shader. However, we do have a placeholder for storing these flags:
 int baseColorTexture = -1;
 int emissiveTexture = -1;
 int normalTexture = -1;
 int opacityTexture = -1;
 uint32_t flags = sMaterialFlags_CastShadow |
 sMaterialFlags_ReceiveShadow;
};
4. Before we can save and load materials, we need to revisit another data structure, MeshData. In 
Chapter 5, we introduced a similar struct called MeshData that contained only geometry data. 
Now, let’s add two new member fields – a vector of Material structs (materials) and a vector 
of texture file names (textureFiles):
struct MeshData {
 lvk::VertexInput streams = {};
 vector<uint32_t> indexData;
 vector<uint8_t> vertexData;
 vector<Mesh> meshes;
 vector<BoundingBox> boxes;


 vector<Material> materials;
 vector<std::string> textureFiles;
};
5. Once we have these data structures established, let’s examine the code for loading and saving 
scene materials in shared/Scene/VtxData.cpp. The following loadMeshDataMaterials()
function is responsible for reading a list of materials from a file. We will omit all the error 
checks in this discussion for brevity, but the actual code is much more detailed:
void loadMeshDataMaterials(const char* fileName, MeshData& out) {
 FILE* f = fopen(fileName, "rb");
 uint64_t numMaterials = 0;
 uint64_t materialsSize = 0;
 fread(&numMaterials, 1, sizeof(numMaterials), f);
 fread(&materialsSize, 1, sizeof(materialsSize), f);
 out.materials.resize(numMaterials);
 fread(out.materials.data(), 1, materialsSize, f);
 loadStringList(f, out.textureFiles);
 fclose(f);
}
6. The saving function saveMeshDataMaterials() is quite similar:
void saveMeshDataMaterials(const char* fileName,
 const MeshData& m) {
 FILE* f = fopen(fileName, "wb");
 uint64_t numMaterials = m.materials.size();
 uint64_t materialsSize = m.materials.size() * sizeof(Material);
 fwrite(&numMaterials, 1, sizeof(numMaterials), f);
 fwrite(&materialsSize, 1, sizeof(materialsSize), f);
 fwrite(m.materials.data(), sizeof(Material), numMaterials, f);
 saveStringList(f, m.textureFiles);
 fclose(f);
}
The code uses the helper functions saveStringList(), which appends a list of strings to an opened 
binary file, and loadStringList(). They are located in shared/Utils.cpp.
Now we can take a look at the demo application and explore how it works.


### How it works...

The demo application is located at Chapter08/02_SceneGraph/src/main.cpp.
1. At the start of the program, we check whether our scene is precached. If it isn’t, we load a .gltf
file into our MeshData and Scene data structures and save them for future use. The isMesh...
Valid() helper functions check for the existence and validity of the cache files and can be 
found in shared/Scene/VtxData.cpp:
 const char* fileNameCachedMeshes = ".cache/ch08_orrery.meshes";
 const char* fileNameCachedMaterials = ".cache/ch08_orrery.materials";
 const char* fileNameCachedHierarchy = ".cache/ch08_orrery.scene";
 if (!isMeshDataValid(fileNameCachedMeshes) ||
 !isMeshHierarchyValid(fileNameCachedHierarchy)||
 !isMeshMaterialsValid(fileNameCachedMaterials))
 {
 MeshData meshData;
 Scene ourScene;
2. The loadMeshFile() function is similar to the one described in the recipe Implementing automatic geometry conversion in Chapter 5. This new version implemented in Chapter08/SceneUtils.h
is capable of loading materials and textures. We will explore it further later in this chapter in 
the recipe Implementing automatic material conversion:
 loadMeshFile("data/meshes/orrery/scene.gltf",
 meshData, ourScene, true);
 saveMeshData(fileNameCachedMeshes, meshData);
 saveMeshDataMaterials(
 fileNameCachedMaterials, meshData);
 saveScene(fileNameCachedHierarchy, ourScene);
 }
3. Once the scene is loaded from a .gltf file and precached into our compact binary data formats, 
we can load it easily and proceed with rendering. The helper function loadMeshData() was 
discussed in Chapter 5, and the loadScene() function was covered in the recipe Loading and 
saving a scene graph earlier in this chapter:
 MeshData meshData;
 const MeshFileHeader header =
 loadMeshData(fileNameCachedMeshes, meshData);
 loadMeshDataMaterials(fileNameCachedMaterials, meshData);
 Scene scene;
 loadScene(fileNameCachedHierarchy, scene);


4. Before we enter the application rendering loop, we create a VKMesh object, which is responsible for the rendering of our MeshData and Scene. We will discuss it in the recipe Putting it all 
together into a scene editing application:
 VulkanApp app({
 .initialCameraPos = vec3(-0.88f, 1.26f, 1.07f),
 .initialCameraTarget = vec3(0, -0.6f, 0) });
 unique_ptr<lvk::IContext> ctx(app.ctx_.get());
 const VKMesh mesh(ctx, header, meshData, scene, app.getDepthFormat());
Now let’s take a look at how to import the materials from Assimp. The next recipe shows how to extract and pack the values from the Assimp library’s aiMaterial structure to our Material structure.

## Implementing automatic material conversion

In the previous recipe, we learned how to create a runtime data storage format for scene materials 
and how to save and load it from disk. In the recipe Implementing the glTF 2.0 metallic-roughness shading model in Chapter 6, we explored how to load PBR material properties. Now, let’s do another quick 
intermezzo and learn how to extract material properties from Assimp data structures and convert 
textures for the demo applications in this chapter.

### Getting ready

Refer to the previous recipe, Implementing a material system, where we learned how to load and store 
multiple meshes with different materials. Now, it’s time to explain how to import material data from 
popular 3D asset formats.

### How to do it...

Let’s take a look at the convertAIMaterial() function from Chapter08/VKMesh08.h. It retrieves all 
the required parameters from Assimp’s aiMaterial structure and returns a Material object suitable 
for storage.
1. Each texture is later addressed by an integer identifier. We store a list of texture filenames in 
the files parameter. The opacityMap parameter contains a list of textures that need to be 
combined with opacity/transparency maps:
Material convertAIMaterial(
 const aiMaterial* M,
 vector<std::string>& files,
 vector<std::string>& opacityMaps)
{
 Material D;
 aiColor4D Color;


2. The Assimp API provides getter functions that we use to extract individual color parameters. 
These functions allow us to read all the required color data. Since most of the code for handling different color values is quite similar, we won’t include it all here to keep things concise:
 if (aiGetMaterialColor(M, AI_MATKEY_COLOR_AMBIENT,
 &Color) == AI_SUCCESS)
 {
 D.emissiveFactor = {
 Color.r, Color.g, Color.b, Color.a };
 if (D.emissiveFactor.w > 1.0f)
 D.emissiveFactor.w = 1.0f;
 }
 // ...
3. After reading the colors and various scalar factors, we move on to handling textures. All textures for our materials are stored in external files, and the filenames are extracted using the 
aiGetMaterialTexture() function:
 aiString path;
 aiTextureMapping mapping;
 unsigned int uvIndex = 0;
 float blend = 1.0f;
 aiTextureOp textureOp = aiTextureOp_Add;
 aiTextureMapMode textureMapMode[2] = {
 aiTextureMapMode_Wrap,
 aiTextureMapMode_Wrap };
 unsigned int textureFlags = 0;
4. This function requires several parameters, but for simplicity, we mostly ignore them in our 
converter. The first texture is an emissive map, and we use the addUnique() function to add 
the texture file to our files list:
 if (aiGetMaterialTexture(M,
 aiTextureType_EMISSIVE, 0, &path,
 &mapping, &uvIndex, &blend, &textureOp,
 textureMapMode,
 &textureFlags) == AI_SUCCESS)
 {
 D.emissiveTexture = addUnique(files, path.C_Str());
 }


5. The diffuse map is stored in the baseColorTexture field of our material structure. Here, we 
apply some ad hoc heuristics based on the material name to mark the material as transparent. 
This is necessary to improve the appearance of the Bistro model by customizing some materials:
 if (aiGetMaterialTexture(M, aiTextureType_DIFFUSE,
 0, &path, &mapping, &uvIndex, &blend,
 &textureOp, textureMapMode,
 &textureFlags) == AI_SUCCESS)
 {
 D.baseColorTexture = addUnique(files, path.C_Str());
 const string albedoMap = string(path.C_Str());
 if (albedoMap.find("grey_30") != albedoMap.npos)
 D.flags |= sMaterialFlags_Transparent;
 }
6. The normal map can be extracted from either the aiTextureType_NORMALS property or 
aiTextureType_HEIGHT in aiMaterial. We check for the presence of an aiTextureType_
NORMALS texture map and store the texture index in the normalTexture field:
 if (aiGetMaterialTexture(M, aiTextureType_NORMALS,
 0, &path, &mapping, &uvIndex, &blend,
 &textureOp, textureMapMode,
 &textureFlags) == AI_SUCCESS)
 {
 D.normalTexture = addUnique(files, path.C_Str());
 }
7. If there is no tangent space normal map, we should check if a heightmap texture is present, 
which can be converted to a normal map at a later stage of the conversion process:
 if ( (D.normalTexture == -1) &&
 (aiGetMaterialTexture(M,
 aiTextureType_HEIGHT, 0, &path,
 &mapping, &uvIndex, &blend, &textureOp,
 textureMapMode,
 &textureFlags) == AI_SUCCESS) )
 {
 D.normalTexture = addUnique(files, path.C_Str());
 }


8. The final map we use is the opacity map, with each one stored in the output opacityMaps array. 
We pack the opacity maps into the alpha channel of our albedo textures:
 if (aiGetMaterialTexture(M, aiTextureType_OPACITY,
 0, &path, &mapping, &uvIndex, &blend,
 &textureOp, textureMapMode,
 &textureFlags) == AI_SUCCESS)
 {
 D.opacityTexture = addUnique(opacityMaps, path.C_Str());
 D.alphaTest = 0.5f;
 }
9. The final part of the material conversion process uses heuristics to infer material properties 
based on the material’s name. In our largest test scene, we only check for glass-like materials, 
but other common names like “gold” or “silver” can also be used to assign metallic coefficients 
and albedo colors. This simple trick helps improve the appearance of the test scene. Once 
complete, the Material object is returned for further processing:
 aiString Name;
 std::string materialName;
 auto name = [&materialName](
 const char* substr) -> bool {
 return materialName.find(substr) != std::string::npos;
 };
 if (name("MASTER_Glass_Clean") ||
 name("MenuSign_02_Glass") ||
 name("Vespa_Headlight")) {
 D.alphaTest = 0.75f;
 D.transparencyFactor = 0.2f;
 D.flags |= sMaterialFlags_Transparent;
 } else if (name("MASTER_Glass_Exterior") ||
 name("MASTER_Focus_Glass")) {
 D.alphaTest = 0.75f;
 D.transparencyFactor = 0.3f;
 D.flags |= sMaterialFlags_Transparent;
 } else if (name("MASTER_Frosted_Glass") {
 // ...
 }
 return D;
}
10. One last thing to note is the addUnique() function, which populates the list of texture files. It 
checks if a filename is already in the collection. If the filename isn’t there, it’s added, and its 
index is returned. Otherwise, the index of the previously added texture file is returned:


int addUnique(
 vector<std::string>& files, const std::string& file)
{
 if (file.empty()) return -1;
 const auto i = std::find(std::begin(files), std::end(files), file);
 if (i != files.end())
 return (int)std::distance(files.begin(), i);
 files.push_back(file);
 return (int)files.size() - 1;
}
We have now read all the material properties from Assimp and built a list of texture filenames needed 
to render our scene. Since the 3D models come from the internet, these textures can vary significantly 
in size and be stored in various uncompressed image formats. For runtime rendering, we want consistent texture sizes, and we also want to compress them into a GPU-friendly format, as we did in the 
recipe Compressing textures into the BC7 format in Chapter 1. This conversion is handled by the conver
tAndDownscaleAllTextures() function. Let’s take a look at how it works.

### How it works...

The helper function convertAndDownscaleAllTextures() is defined in Chapter08/SceneUtils.h. 
It iterates through all the texture files in a multi-threaded manner and calls the convertTexture()
function for each texture file. This file defines two C++ macros: DEMO_TEXTURE_MAX_SIZE and DEMO_
TEXTURE_CACHE_FOLDER. These specify the maximum texture dimensions for rescaling and the output 
cache folder, respectively. We’ll redefine these macros in later chapters to reuse this code with different 
texture resolutions.
1. This routine takes several parameters – a list of material descriptions, an output directory for 
the converted textures basePath, and containers for all the texture filenames and opacity maps:
void convertAndDownscaleAllTextures(
 const vector<Material>& materials,
 const std::string& basePath,
 vector<std::string>& files,
 vector<std::string>& opacityMaps)
{
Note
The addUnique() function has a complexity of O(n²). This is acceptable in this 
case because it’s only used during the material conversion stage, where the number 
of texture files is relatively small.


2. Each opacity map is combined with the base color map. To maintain the correspondence 
between the opacity map list and the global texture indices, we use a standard C++ hash table. 
We iterate through all the materials and check if they have both an opacity map and an albedo 
map. If both maps are present, we associate the opacity map with the albedo map:
 std::unordered_map<std::string, uint32_t>
 opacityMapIndices(files.size());
 for (const auto& m : materials) {
 if (m.opacityTexture != -1 &&
 m.baseColorTexture != -1)
 opacityMapIndices[files[m.baseColorTexture]] =
 (uint32_t)m.opacityTexture;
 }
3. The following lambda function takes a source texture filename from the 3D model and returns 
a modified cached texture filename. It also performs the conversion of the texture data by 
calling the convertTexture() function, which we will explore in a moment:
 auto converter = [&](const std::string& s) -> std::string {
 return convertTexture(
 s, basePath, opacityMapIndices, opacityMaps);
 };
4. Now, we can use the std::transform() algorithm with the std::execution::par argument 
to convert all the texture files in a multi-threaded manner:
 std::transform(std::execution::par,
 std::begin(files), std::end(files),
 std::begin(files), converter);
}
The std::execution::par parameter is a C++20 feature that enables parallel processing of 
the container. Since converting the texture data can be a lengthy process, this simple parallelization may significantly reduce our processing time.
A single texture map is converted to our runtime data format using the convertTexture() routine 
and is stored as a .ktx file. Let’s take a look at how it works:
1. The function takes a source texture filename from the 3D model, a basePath folder containing 
the 3D model file, and a collection of mappings for the opacity map indices: 
std::string convertTexture(
 const std::string& file,
 const std::string& basePath,
 unordered_map<std::string, uint32_t>& opacityMapIndices,
 const vector<std::string>& opacityMaps)
{


2. All of our output textures will have a maximum size of 512x512 pixels, controlled by the macro 
DEMO_TEXTURE_MAX_SIZE, to make sure our demos run fast. We also create a destination folder 
to store the cached textures:
 const int maxNewWidth = DEMO_TEXTURE_MAX_SIZE;
 const int maxNewHeight = DEMO_TEXTURE_MAX_SIZE;
 namespace fs = std::filesystem;
 if (!fs::exists(DEMO_TEXTURE_CACHE_FOLDER)) {
 fs::create_directories(DEMO_TEXTURE_CACHE_FOLDER);
 }
3. The output filename is created by concatenating a fixed output directory with the source filename, replacing all path separators with double underscores. To ensure compatibility across 
Windows, Linux, and macOS, we replace all path separators with the / symbol:
 const string srcFile = replaceAll(basePath + file, "\\", "/");
 const string newFile = std::string(
 DEMO_TEXTURE_CACHE_FOLDER) +
 lowercaseString(replaceAll(
 replaceAll(srcFile, "..", "__"), "/", "__")+std::string("__rescaled")
 ) + std::string(".ktx");
4. As we did in the previous chapters, we use the stb_image library to load textures. We enforce 
that the loaded image is in RGBA format, even if there is no opacity information. This is a 
convenient shortcut that simplifies our texture handling code significantly:
 int origWidth, origHeight, texChannels;
 stbi_uc* pixels =
 stbi_load(fixTextureFile(srcFile).c_str(),
 &origWidth, &origHeight, &texChannels,
 STBI_rgb_alpha);
 uint8_t* src = pixels;
 texChannels = STBI_rgb_alpha;
 uint8_t* src = pixels;
 texChannels = STBI_rgb_alpha;
Note
The fixTextureFile() function addresses situations where the 3D model material data references texture files with an incorrect case in their filenames. For 
instance, the .mtl file might contain map_Ka Texture01.png, while the actual 
filename on the filesystem is in lowercase, texture01.png. This function helps 
us resolve naming inconsistencies in the Bistro scene on Linux.


5. If the texture fails to load, we set our empty temporary array as the input data to prevent 
having to exit at this point:
 std::vector<uint8_t> tmpImage(maxNewWidth * maxNewHeight * 4);
 if (!src) {
 printf("Failed to load [%s] texture\n", srcFile.c_str());
 origWidth = maxNewWidth;
 origHeight = maxNewHeight;
 texChannels = STBI_rgb_alpha;
 src = tmpImage.data();
 }
6. If the texture has an associated opacity map stored in the hash table, we load that opacity map 
and combine its contents with the albedo map. As with the source texture file, we replace 
path separators to ensure cross-platform compatibility. The opacity map is loaded as a simple 
grayscale image:
 if (opacityMapIndices.count(file) > 0) {
 const std::string opacityMapFile =
 replaceAll(basePath + opacityMaps[opacityMapIndices[file]], "\\", "/");
 int opacityWidth, opacityHeight;
 stbi_uc* opacityPixels = stbi_load(
 fixTextureFile(opacityMapFile).c_str(),
 &opacityWidth, &opacityHeight, nullptr, 1);
7. After successfully loading the opacity map with the correct dimensions, we store the opacity 
values in the alpha component of the albedo texture. The stb_image library uses explicit 
memory management, so we free the loaded opacity map manually:
 assert(opacityPixels);
 assert(origWidth == opacityWidth);
 assert(origHeight == opacityHeight);
 if (opacityPixels) {
 for (int y = 0; y != opacityHeight; y++)
 for (int x = 0; x != opacityWidth; x++) {
 int idx = y * opacityWidth + x;
 src[idx * texChannels + 3] = opacityPixels[idx];
 }
 }
 stbi_image_free(opacityPixels);
 }
8. Once the texture is combined with the optional opacity map, we can downscale it, generate all 
the mip levels, and compress it into the BC7 format. We will generate all the mip levels ourselves:


 const int newW = std::min(origWidth, maxNewWidth);
 const int newH = std::min(origHeight, maxNewHeight);
 const uint32_t numMipLevels = lvk::calcNumMipLevels(newW, newH);
 ktxTextureCreateInfo createInfoKTX2 = {
 .glInternalformat = GL_RGBA8,
 .vkFormat = VK_FORMAT_R8G8B8A8_UNORM,
 .baseWidth = (uint32_t)newW,
 .baseHeight = (uint32_t)newH,
 .baseDepth = 1u,
 .numDimensions = 2u,
 .numLevels = numMipLevels,
 .numLayers = 1u,
 .numFaces = 1u,
 .generateMipmaps = KTX_FALSE,
 };
 ktxTexture2* textureKTX2 = nullptr;
 ktxTexture2_Create(&createInfoKTX2,
 KTX_TEXTURE_CREATE_ALLOC_STORAGE, &textureKTX2);
9. Here’s the loop that generates all the mip levels from the full-sized texture. The stb_image_
resize library offers a simple stbir_resize_uint8_linear() function to rescale an image 
without significant loss of quality:
 int w = newW;
 int h = newH;
 for (uint32_t i = 0; i != numMipLevels; ++i) {
 size_t offset = 0;
 ktxTexture_GetImageOffset(
 ktxTexture(textureKTX2), i, 0, 0, &offset);
 stbir_resize_uint8_linear( (const uint8_t*)src,
 origWidth, origHeight, 0,
 ktxTexture_GetData(ktxTexture(textureKTX2)) + offset,
 w, h, 0, STBIR_RGBA);
 h = h > 1 ? h >> 1 : 1;
 w = w > 1 ? w >> 1 : 1;
 }
10. Then, we compress the entire image into the BC7 format using the KTX-Software library. This 
involves two steps – first, compressing the image into the Basis format, and then transcoding 
it into BC7:
 ktxTexture2_CompressBasis(textureKTX2, 255);
 ktxTexture2_TranscodeBasis(textureKTX2, KTX_TTF_BC7_RGBA, 0);
 ktxTextureCreateInfo createInfoKTX1 = {


 .glInternalformat = GL_COMPRESSED_RGBA_BPTC_UNORM,
 .vkFormat = VK_FORMAT_BC7_UNORM_BLOCK,
 .baseWidth = (uint32_t)newW,
 .baseHeight = (uint32_t)newH,
 .baseDepth = 1u,
 .numDimensions = 2u,
 .numLevels = numMipLevels,
 .numLayers = 1u,
 .numFaces = 1u,
 .generateMipmaps = KTX_FALSE,
 };
 ktxTexture1* textureKTX1 = nullptr;
 ktxTexture1_Create(&createInfoKTX1,
 KTX_TEXTURE_CREATE_ALLOC_STORAGE, &textureKTX1);
 for (uint32_t i = 0; i != numMipLevels; ++i) {
 size_t offset1 = 0;
 ktxTexture_GetImageOffset(
 ktxTexture(textureKTX1), i, 0, 0, &offset1);
 size_t offset2 = 0;
 ktxTexture_GetImageOffset(
 ktxTexture(textureKTX2), i, 0, 0, &offset2);
 memcpy( ktxTexture_GetData(
 ktxTexture(textureKTX1)) + offset1,
 ktxTexture_GetData(
 ktxTexture(textureKTX2)) + offset2,
 ktxTexture_GetImageSize(ktxTexture(textureKTX1), i) );
 }
11. Finally, we save the resulting .ktx file and release all resources. Regardless of the conversion 
outcome, we return the new texture filename: 
 ktxTexture_WriteToNamedFile(
 ktxTexture(textureKTX1), newFile.c_str());
 ktxTexture_Destroy(ktxTexture(textureKTX1));
 ktxTexture_Destroy(ktxTexture(textureKTX2));
 return newFile;
}
This ensures that even if some textures from the original 3D model are missing, the converted 
dataset remains valid and requires significantly fewer runtime checks.


There’s more...
This relatively lengthy recipe has detailed all the necessary routines for retrieving and precaching 
material and texture data from external 3D assets. This will greatly enhance loading speeds for large 
3D models, such as the Bistro model, which is essential for interactive debugging of complex graphics 
applications. Now, let’s switch back to Vulkan and explore how to render our scene data with LightweightVK.
Using descriptor indexing and arrays of textures in 
Vulkan
Before we can use the material system described earlier in this chapter on the GPU, as discussed in the 
recipe Implementing a material system, let’s first explore some Vulkan functionality that is essential for 
conveniently handling numerous materials with multiple textures on the GPU. Descriptor indexing 
is a highly useful feature that became mandatory in Vulkan 1.3 and is now widely supported on both 
desktop and mobile devices. We briefly discussed this in Chapter 3, in the recipe Using Vulkan descriptor 
indexing, where we explored how LightweightVK manages descriptor sets. Descriptor indexing allows 
for the creation of unbounded descriptor sets and the use of non-uniform dynamic indexing to access 
textures within them. This enables materials to be stored in shader storage buffers, with each one 
referencing the necessary textures using integer IDs. These IDs can be retrieved from buffers and 
used directly to index into the appropriate descriptor set containing all the textures needed by the 
application. Now, let’s take a closer look at how to use this feature.
Our example app uses three different explosion animations released by Unity Technologies under the 
liberal CC0 license: https://blogs.unity3d.com/2016/11/28/free-vfx-image-sequences-flipbooks.

### Getting ready

Make sure to revisit the recipe Using Vulkan descriptor indexing in Chapter 3 to understand the underlying Vulkan implementation.
The source code for this recipe is located at Chapter08/01_DescriptorIndexing. All the textures we 
use are stored in the deps/src/explosion folder.

### How to do it...

Let’s build an app that renders animated explosions using the flipbook technique. We will store 100 
images representing the explosion frames, load them into GPU memory as textures, and apply the 
appropriate texture based on the current animation time. Since no binding is needed, we just need 
to update the texture ID at the right time and pass it to our GLSL shaders.


The application renders an animated explosion every time the user clicks anywhere inside the window. 
Multiple explosions, each using a different flipbook, can be rendered at the same time. Let’s explore 
the C++ code from Chapter08/01_DescriptorIndexing/src/main.cpp to learn how to do it.
1. First, we need to define some constants and structures to represent our animations. Here, 
position represents the screen location of an explosion, startTime marks the timestamp 
when the animation started, time indicates the current animation time, textureIndex is the 
index of the current texture in the flipbook, and firstFrame is the index of the first frame of 
the current flipbook in the texture array:
const double kAnimationFPS = 50.0;
const uint32_t kNumFlipbooks = 3;
const uint32_t kNumFlipbookFrames = 100;
struct AnimationState {
 vec2 position = vec2(0);
 double startTime = 0;
 float time = 0;
 uint32_t textureIndex = 0;
 uint32_t firstFrame = 0;
};
2. The g_Animations vector holds the current state of animations for all visible explosions and 
is updated every frame. The g_AnimationsKeyframe vector saves the animation state of all explosions when we want to pause the simulation and manually adjust the current animation by 
offsetting it from stored keyframes using the ImGui UI. We’ll explain this further in a moment. 
The timelineOffset parameter is used to offset the time from the keyframe values “locked” 
in g_AnimationsKeyframe:
std::vector<AnimationState> g_Animations;
std::vector<AnimationState> g_AnimationsKeyframe;
float timelineOffset = 0.0f;
bool showTimeline = false;
3. Here’s the animation update logic placed into a separate function, updateAnimations(). The 
current texture index is updated for each animation based on its start time. As we go through 
all the animations, we can safely remove the ones that are finished. Instead of using the swapand-pop trick to remove an element from the vector, which can cause distracting Z-fighting 
where animations suddenly pop in front of one another, we use a simple, straightforward 
removal with erase():
void updateAnimations(float deltaSeconds) {
 for (size_t i = 0; i < g_Animations.size();) {
 g_Animations[i].time += deltaSeconds;
 g_Animations[i].textureIndex =
 g_Animations[i].firstFrame +


 (uint32_t)(kAnimationFPS * g_Animations[i].time);
 if (g_Animations[i].textureIndex >=
 kNumFlipbookFrames + g_Animations[i].firstFrame)
 g_Animations.erase(g_Animations.begin() + i);
 else i++;
 }
}
4. The setAnimationsOffset() function takes a time offset and applies it to the values in g_
AnimationsKeyframe to compute the new values for g_Animations:
void setAnimationsOffset(float offset) {
 for (size_t i = 0; i < g_Animations.size(); i++) {
 g_Animations[i].time = std::max(
 g_AnimationsKeyframe[i].time + offset, 0.0f);
 g_Animations[i].textureIndex =
 g_Animations[i].firstFrame + (uint32_t)
 (kAnimationFPS * g_Animations[i].time);
 g_Animations[i].textureIndex = std::min(
 g_Animations[i].textureIndex,
 kNumFlipbookFrames + g_Animations[i].firstFrame - 1);
 }
}
Now that all the utilities are in place, we’re ready to dive into the main() function.
1. First, we create our app and load all the textures:
int main() {
 VulkanApp app;
 unique_ptr<lvk::IContext> ctx(app.ctx_.get());
2. We have 3 different explosion types, each with 100 frames defined as kNumFlipbooks and 
kNumFlipbookFrames, respectively. All the texture files are loaded from the deps/src/
explosion/ folder and its subfolders – explosion/, explosion1/, and explosion2/:
 std::vector<Holder<TextureHandle>> textures;
 textures.reserve(kNumFlipbooks * kNumFlipbookFrames);
 for (uint32_t book = 0;
 book != kNumFlipbooks; book++) {
 for (uint32_t frame = 0;
 frame != kNumFlipbookFrames; frame++) {
 char fname[1024];
 snprintf(fname, sizeof(fname),
 "deps/src/explosion/explosion%01u/explosion%02u-frame%03u.tga",


 book, book, frame + 1);
 textures.emplace_back(
 loadTexture(ctx, fname));
 }
 }
3. Next, we load the vertex and fragment shaders, which we’ll examine shortly.
 Holder<ShaderModuleHandle> vert =
 loadShaderModule(ctx,
 "Chapter08/01_DescriptorIndexing/src/main.vert");
 Holder<ShaderModuleHandle> frag =
 loadShaderModule(ctx,
 "Chapter08/01_DescriptorIndexing/src/main.frag");
4. The rendering pipeline is set up using alpha blending mode with BlendFactor_SrcAlpha and 
BlendFactor_OneMinusSrcAlpha, allowing us to render transparent texels correctly:
 Holder<RenderPipelineHandle> pipelineQuad =
 ctx->createRenderPipeline({
 .topology = lvk::Topology_TriangleStrip,
 .smVert = vert,
 .smFrag = frag,
 .color = { {
 .format = ctx->getSwapchainFormat(),
 .blendEnabled = true,
 .srcRGBBlendFactor = lvk::BlendFactor_SrcAlpha,
 .dstRGBBlendFactor = lvk::BlendFactor_OneMinusSrcAlpha,
 } },
 });
5. Let’s put the first explosion in the center of the screen:
 g_Animations.push_back(AnimationState{
 .position = vec2(0.5f, 0.5f),
 .startTime = glfwGetTime(),
 .textureIndex = 0,
 .firstFrame = kNumFlipbookFrames * (uint32_t)(rand() % 3),
 });
6. Before entering the rendering loop, we’ll set up the GLFW mouse and key callbacks. The mouse 
callback will allow us to add a new explosion to the screen with a mouse click. The flipbook 
starting offset firstFrame is selected randomly for each explosion:
 app.addMouseButtonCallback([](
 GLFWwindow* window,


 int button, int action, int mods)
 {
 VulkanApp* app = (VulkanApp*)
 glfwGetWindowUserPointer(window); ImGuiIO& io = ImGui::GetIO();
 if (button == GLFW_MOUSE_BUTTON_LEFT &&
 action == GLFW_PRESS &&
 !io.WantCaptureMouse && !showTimeline) {
 g_Animations.push_back(AnimationState{
 .position = app->mouseState_.pos,
 .startTime = glfwGetTime(),
 .textureIndex = 0,
 .firstFrame = kNumFlipbookFrames *
 (uint32_t)(rand() % kNumFlipbooks),
 });
 }
 });
7. The key callback is used to toggle the timeline UI and “lock” the current state of all explosion 
animations as a keyframe into the g_AnimationKeyframe vector:
 app.addKeyCallback([](GLFWwindow* window,
 int key, int scancode, int action, int mods) {
 ImGuiIO& io = ImGui::GetIO();
 const bool pressed = action != GLFW_RELEASE;
 if (key == GLFW_KEY_SPACE && pressed &&
 !io.WantCaptureKeyboard)
 showTimeline = !showTimeline;
8. Save the current animation state as a keyframe. If there are no animations playing, disable 
the timeline UI:
 if (showTimeline) {
 timelineOffset = 0.0f;
 g_AnimationsKeyframe = g_Animations;
 }
 if (g_Animations.empty())
 showTimeline = false;
 });
Now that everything is set up, let’s dive into the rendering loop and see how it all comes together to 
render the animated explosions.


### How it works...

The application’s main loop looks as follows:
1. When the timeline UI is active, we pause the animation updates:
app.run([&](uint32_t width, uint32_t height,
 float aspectRatio, float deltaSeconds) {
 if (!showTimeline) updateAnimations(deltaSeconds);
2. Let’s introduce a helper lambda to calculate transparency at the start and end of animations 
using the smoothstep() function. This way, our explosions will appear and disappear smoothly, 
without any distractions. Here, p1 represents the appearance transition for the first 10% of the 
animation, and p2 represents the disappearance transition for the last 20% of the animation:
 auto easing = [](float t) -> float {
 const float p1 = 0.1f;
 const float p2 = 0.8f;
 if (t <= p1)
 return glm::smoothstep(0.0f, 1.0f, t / p1);
 if (t >= p2)
 return glm::smoothstep(1.0f, 0.0f, (t - p2) / (1.0f - p2));
 return 1.0f;
 };
3. The render passes and framebuffer configurations are straightforward. As usual, we acquire 
a command buffer for rendering:
 const lvk::RenderPass renderPass = {
 .color = {{ .loadOp = lvk::LoadOp_Clear,
 .clearColor = { 1, 1, 1, 1 } } },
 };
 const lvk::Framebuffer framebuffer = {
 .color = {{ .texture = ctx->getCurrentSwapchainTexture() }},
 };
 ICommandBuffer& buf = ctx->acquireCommandBuffer();
 buf.cmdBeginRendering(renderPass, framebuffer);
 buf.cmdBindRenderPipeline(pipelineQuad);
4. We render each explosion as a quad, and GLSL shader parameters for each explosion are passed 
using Vulkan push constants. The time in seconds (s.time) is converted to normalized time, t, 
ranging from 0 to 1. The current textureIndex is converted to the LightweightVK texture index:
 for (const AnimationState& s : g_Animations) {
 const float t = s.time / (kNumFlipbookFrames / kAnimationFPS);
 const struct {


 mat4 proj;
 uint32_t textureId;
 vec2 pos;
 vec2 size;
 float alphaScale;
 } pc {
 .proj = glm::ortho(
 0.0f, float(width), 0.0f, float(height)),
 .textureId = textures[s.textureIndex].index(),
 .pos = s.position * vec2(width, height),
 .size = vec2(height * 0.5f),
 .alphaScale = easing(t),
 };
 buf.cmdPushConstants(pc);
 buf.cmdDraw(4);
 }
5. After rendering all the explosions, we can render our ImGui UI, which includes the FPS counter 
and hints:
 app.imgui_->beginFrame(framebuffer);
 app.drawFPS();
 ImGui::SetNextWindowPos(ImVec2(10, 10));
 ImGui::Begin("Hints:", nullptr,
 ImGuiWindowFlags_AlwaysAutoResize |
 ImGuiWindowFlags_NoFocusOnAppearing |
 ImGuiWindowFlags_NoInputs |
 ImGuiWindowFlags_NoCollapse);
 if (showTimeline) {
 ImGui::Text("SPACE - toggle timeline");
 } else {
Note
Curious readers may notice several inefficiencies in this code. A faster approach 
would be to store all the explosion data in a buffer and then call cmdDraw() with 
the number of instances equal to the number of explosions. This is true. However, it was a deliberate choice. Since the number of active explosions in this 
demo is very low – capping at a few dozen at most – the performance gain would 
be modest, but it would introduce extra complexity, such as managing multiple 
round-robin uniform buffers and ensuring proper synchronization. We leave this 
as an exercise for you.


 ImGui::Text("SPACE - toggle timeline");
 ImGui::Text("Left click - set an explosion");
 }
 ImGui::End();
6. The timeline editing UI can be rendered in a similar way:
 if (showTimeline) {
 const ImGuiViewport* v = ImGui::GetMainViewport();
 ImGui::SetNextWindowContentSize({ v->Size.x - 520, 0 });
 ImGui::SetNextWindowPos(ImVec2(350, 10), ImGuiCond_Always);
 ImGui::Begin("Timeline:", nullptr,
 ImGuiWindowFlags_NoCollapse |
 ImGuiWindowFlags_NoResize);
7. Each time we move the slider, the offset value is used to calculate the new animation state 
based on the “locked” keyframe, which we stored earlier in g_AnimationsKeyframe when 
toggling the timeline editing UI:
 if (ImGui::SliderFloat("Time offset", &timelineOffset, -2.0f, +2.0f))
 {
 setAnimationsOffset(timelineOffset);
 }
 ImGui::End();
 }
 app.imgui_->endFrame(buf);
 buf.cmdEndRendering();
 ctx->submit(
 buf, ctx->getCurrentSwapchainTexture());
});
That covers the C++ part. Now, let’s take a look at the GLSL shaders for our application.
The vertex shader Chapter08/01_DescriptorIndexing/src/main.vert to render our textured quads 
looks the following way:
1. The PerFrameData structure corresponds to the C++ push constants we passed into the shaders 
and is shared between the vertex and fragment shaders. It is declared in the Chapter08/01_
DescriptorIndexing/src/common.sp file:
#version 460
// #include <Chapter08/01_DescriptorIndexing/src/common.sp>
layout(push_constant) uniform PerFrameData {
 mat4 proj;
 uint textureId;
 float x;


 float y;
 float width;
 float height;
 float alphaScale;
} pc;
2. We use the programmable vertex pulling technique to calculate quad vertices using the value 
of gl_VertexIndex:
layout (location=0) out vec2 uv;
const vec2 pos[4] = vec2[4](
 vec2( 0.5, -0.5),
 vec2( 0.5, 0.5),
 vec2(-0.5, -0.5),
 vec2(-0.5, 0.5)
);
void main() {
 uv = pos[gl_VertexIndex] + vec2(0.5);
3. The clip-space position is calculated by scaling and offsetting the unit quad stored in pos using 
the values of width, height, and the xy screen position of the explosion:
 vec2 p = vec2(pc.x, pc.y) +
 pos[gl_VertexIndex] * vec2(pc.width, pc.height);
 gl_Position = pc.proj * vec4(p, 0.0, 1.0);
}
4. The fragment shader Chapter08/01_DescriptorIndexing/src/main.frag is straightforward. 
All we need to do here is scale the texture’s alpha value by the alphaScale value that we obtained in C++ using the easing function:
#include <Chapter08/01_DescriptorIndexing/src/common.sp>
layout (location=0) in vec2 uv;
layout (location=0) out vec4 out_FragColor;
void main() {
 out_FragColor = vec4(vec3(1), pc.alphaScale) *
 textureBindless2D(pc.textureId, 0, uv);
}


Now we can run our application. Click a few times in the window and press the Spacebar to see something similar to the screenshot below:
Figure 8.3: Animated explosions using descriptor indexing and arrays of textures
You can interact with the timeline UI by moving the slider, allowing you to control the animation’s 
progress. By dragging the slider, you can rewind and fast-forward the animation, giving you precise 
control over its playback.
Now, let’s move on to the next recipe and explore how to use descriptor indexing to render our scene 
using the material system we described in Implementing a material system.

## Implementing indirect rendering with Vulkan

In the previous chapters, we covered geometry rendering with basic texturing. In the previous recipe 
Using descriptor indexing and arrays of textures in Vulkan, we learned how to wrangle multiple textures. 
This recipe explains how to use Vulkan’s indirect rendering feature to render the scene data discussed 
earlier in the chapter, specifically in the recipes Loading and saving a scene graph and Implementing a 
material system. While this recipe doesn’t have a standalone example app, all the helper code provided 
here will be used in the upcoming recipes. 
Indirect rendering allows a significant part of scene traversal to be precalculated and offloaded from 
the CPU to the GPU, which can greatly boost performance. The key idea is to organize all scene data 
into arrays that shaders can access using integer IDs, eliminating the need for any API state changes 
during rendering. The indirect buffer generated by the application contains an array of fixed-size 
structs, with each struct holding the parameters for a single draw command. Data representation is 
the key here. Let’s explore how to implement this approach for our scene.


### Getting ready

Be sure to revisit the recipes on our scene graph data structures, especially Loading and saving a scene 
graph and Implementing a material system.
This recipe explains the source code for the VKMesh helper class, which is defined in Chapter08/
VKMesh08.h.

### How to do it...

In the recipe Implementing a material system, we learned how to manage materials for the entire scene, 
including textures. The data structures introduced there are intended for CPU-based scene processing. 
Now, let’s introduce a helper function, convertToGPUMaterial(), which converts the CPU material 
representation into a GPU-friendly format.
1. We have simplified The convertToGPUMaterial() function by leaving out the full glTF 2.0 PBR 
material properties. Instead, we’ll focus on managing textures to keep things concise. The 
TextureCache container holds the loaded textures, while the TextureFiles container stores 
the filenames. Both containers are indexed by a textureId and were set up in the recipe Implementing a material system:
using TextureCache = std::vector<Holder<TextureHandle>>;
using TextureFiles = std::vector<std::string>;
GLTFMaterialDataGPU convertToGPUMaterial(
 const std::unique_ptr<lvk::IContext>& ctx,
 const Material& mat,
 const TextureFiles& files,
 TextureCache& cache)
{
 GLTFMaterialDataGPU result = {
 .baseColorFactor = mat.baseColorFactor,
 .metallicRoughnessNormalOcclusion =
 vec4(mat.metallicFactor, mat.roughness, 1, 1),
 .emissiveFactorAlphaCutoff =
 vec4(vec3(mat.emissiveFactor), mat.alphaTest),
 };
2. Let’s introduce a local lambda function to retrieve a texture index from the cache. If the texture with this index isn’t in the cache yet, it will be loaded and added to the cache. This helps 
prevent duplicate textures, as different materials that use the same textures will store the 
same texture IDs:
 auto getTextureFromCache = [&cache, &ctx, &files](
 int textureId) -> uint32_t {
 if (textureId == -1) return 0;
 if (cache.size() <= textureId)


 cache.resize(textureId + 1);
 if (cache[textureId].empty()) {
 cache[textureId] =
 loadTexture(ctx, files[textureId].c_str());
 }
 return cache[textureId].index();
 };
3. Now we can retrieve all the necessary material textures using the cache:
 result.baseColorTexture =
 getTextureFromCache(mat.baseColorTexture);
 result.emissiveTexture =
 getTextureFromCache(mat.emissiveTexture);
 result.normalTexture =
 getTextureFromCache(mat.normalTexture);
 result.transmissionTexture =
 getTextureFromCache(mat.opacityTexture);
 return result;
}
Once this utility function is in place, we can move on to the VKMesh class, which handles all aspects of 
scene rendering. It is responsible for storing GPU buffers for geometry, transformations, materials, 
and other auxiliary data. Despite its name, this class can manage the entire scene, and we’ll see how 
it works shortly.
1. First, we need to store the number of indices in a mesh, as this class handles only indexed 
meshes. It also needs buffers for both indices and vertices:
class VKMesh final {
public:
 const unique_ptr<lvk::IContext>& ctx;
 uint32_t numIndices_ = 0;
 uint32_t numMeshes_ = 0;
 Holder<BufferHandle> bufferIndices_;
 Holder<BufferHandle> bufferVertices_;
2. To make things really fast, we use indirect rendering for our scene. The indirect buffer will 
contain an array of VkDrawIndexedIndirectCommand structures, one for each individual mesh 
in the scene. The transforms buffer contains an array of mat4 model-to-world matrices for the 
entire scene. These matrices were generated in the recipe Implementing transformation trees:


 Holder<BufferHandle> bufferIndirect_;
 Holder<BufferHandle> bufferTransforms_;
3. For each mesh, we need to store its transform index and material index. Let’s introduce a 
DrawData struct and store an array of these in the bufferDrawData_ buffer. This array can be 
accessed from GLSL shaders using the gl_BaseInstance built-in variable to retrieve the appropriate indices. We’ll explore how this works when we begin populating the bufferIndirect_
buffer:
 struct DrawData {
 uint32_t transformId;
 uint32_t materialId;
 };
 Holder<BufferHandle> bufferDrawData_;
4. The materials buffer holds an array of GLTFMaterialDataGPU structs, which we discussed earlier. 
The remaining member fields contain vertex and fragment shaders, as well as two rendering 
pipelines – one for standard rendering and the other for wireframe rendering. We also store 
the texture cache data here:
 Holder<BufferHandle> bufferMaterials_;
 Holder<ShaderModuleHandle> vert_;
 Holder<ShaderModuleHandle> frag_;
 Holder<RenderPipelineHandle> pipeline_;
 Holder<RenderPipelineHandle> pipelineWireframe_;
 TextureFiles textureFiles_;
 mutable TextureCache textureCache_;
Note
Here’s a quick look at the VkDrawIndexedIndirectCommand declaration, which 
corresponds to the parameters of one vkCmdDrawIndexedIndirect() draw call:
struct VkDrawIndexedIndirectCommand {
 uint32_t indexCount;
 uint32_t instanceCount;
 uint32_t firstIndex;
 int32_t vertexOffset;
 uint32_t firstInstance;
};
We’ve already used the firstInstance parameter to pass an integer index into 
GLSL shaders through the gl_BaseInstance built-in variable. We’ll use the same 
technique again, but this time, for a series of draw calls to render the entire scene 
efficiently.


5. The constructor of the VKMesh class takes parameters representing the entire scene, which 
were loaded in the recipe Loading and saving a scene graph. The depth-buffer format is application-dependent and is passed in from outside the class. The numSamples parameter is used 
for MSAA and will be explained in the Implementing MSAA in Vulkan recipe in Chapter 10. For 
now, we will use the default value, 1:
 VKMesh(const std::unique_ptr<lvk::IContext>& ctx,
 const MeshData& meshData,
 const Scene& scene,
 lvk::Format colorFormat,
 lvk::Format depthFormat,
 uint32_t numSamples = 1)
 : ctx(ctx)
 , numIndices_((uint32_t)meshData.indexData.size())
 , numMeshes_((uint32_t)meshData.meshes.size())
 , textureFiles_(meshData.textureFiles) {
6. The MeshFileHeader and MeshData structs already contain the packed geometry vertex and 
index data. All that is left to do is upload them into GPU buffers. Since the geometry is static, 
we don’t need to worry about it anymore. To refresh your memory on how this data was prepared, refer to Chapter 5:
 const MeshFileHeader header = meshData.getMeshFileHeader();
 const uint32_t* indices = meshData.indexData.data();
 const uint8_t* vertexData = meshData.vertexData.data();
 bufferVertices_ = ctx->createBuffer({
 .usage = lvk::BufferUsageBits_Vertex,
 .storage = lvk::StorageType_Device,
 .size = header.vertexDataSize,
 .data = vertexData });
 bufferIndices_ = ctx->createBuffer({
 .usage = lvk::BufferUsageBits_Index,
 .storage = lvk::StorageType_Device,
 .size = header.indexDataSize,
 .data = indices });
7. The model-to-world transformation mat4 matrices are copied from the Scene::globalTransforms
vector. To refresh your memory on how this was done, refer to the recipe Implementing transformation trees:
 bufferTransforms_ = ctx->createBuffer({
 .usage = lvk::BufferUsageBits_Storage,
 .storage = lvk::StorageType_Device,
 .size = scene.globalTransform.size() * sizeof(glm::mat4),
 .data = scene.globalTransform.data() });


8. Now, let’s pack all the materials into a GPU buffer. We iterate through all the materials in 
MeshData::materials and use the helper function convertToGPUMaterial(), which we introduced earlier in this recipe:
 std::vector<GLTFMaterialDataGPU> materials;
 materials.reserve(meshData.materials.size());
 for (const auto& mat : meshData.materials)
 materials.push_back( convertToGPUMaterial(
 ctx, mat, textureFiles_, textureCache_) );
 bufferMaterials_ = ctx->createBuffer({
 .usage = lvk::BufferUsageBits_Storage,
 .storage = lvk::StorageType_Device,
 .size = materials.size() *
 sizeof(decltype(materials)::value_type),
 .data = materials.data() });
9. So far, we’ve uploaded the geometry, transforms, and material data for the entire scene into 
GPU buffers, but we haven’t actually rendered anything yet. For example, our vertex and index 
buffers contain the geometry for all the meshes in the scene, but there’s no information about 
how many of them we actually want to render (if any). Now, we’ll go through all the meshes 
in the scene and create draw commands to render them:
 vector<DrawIndexedIndirectCommand> drawCommands;
 vector<DrawData> drawData;
 const uint32_t numCommands = header.meshCount;
 drawCommands.resize(numCommands);
 drawData.resize(numCommands);
 DrawIndexedIndirectCommand* cmd = drawCommands.data();
 DrawData* dd = drawData.data();
 uint32_t ddIndex = 0;
10. Populate both the indirect commands buffer and the draw data buffer. Note that the baseInstance
parameter holds the index of a DrawData instance, and the corresponding DrawData instance 
contains the transform ID and material ID for that mesh:
 for (auto& i : scene.meshForNode) {
 const Mesh& mesh = meshData.meshes[i.second];
 *cmd++ = { .count = mesh.getLODIndicesCount(0),
 .instanceCount = 1,
 .firstIndex = mesh.indexOffset,
 .baseVertex = mesh.vertexOffset,
 .baseInstance = ddIndex++ };
 *dd++ = { .transformId = i.first,
 .materialId = mesh.materialID };
 }


11. The data for both buffers is now ready, and we can upload it to the GPU:
 bufferIndirect_ = ctx->createBuffer({
 .usage = lvk::BufferUsageBits_Indirect,
 .storage = lvk::StorageType_Device,
 .size = sizeof(DrawIndexedIndirectCommand)*numCommands,
 .data = drawCommands.data() });
 bufferDrawData_ = ctx->createBuffer({
 .usage = lvk::BufferUsageBits_Storage,
 .storage = lvk::StorageType_Device,
 .size = sizeof(DrawData) * numCommands,
 .data = drawData.data() });
12. The rest of the VKMesh constructor code handles the creation of rendering pipelines for both 
normal and wireframe rendering. We’ll include it here without any additional explanations. 
The samplesCount and minSampleShading parameters are used for MSAA and will be explained 
in the Implementing MSAA in Vulkan recipe in Chapter 10:
 vert_ = loadShaderModule(
 ctx, "Chapter08/02_SceneGraph/src/main.vert");
 frag_ = loadShaderModule(
 ctx, "Chapter08/02_SceneGraph/src/main.frag");
 pipeline_ = ctx->createRenderPipeline({
 .vertexInput = meshData.streams,
 .smVert = vert_,
 .smFrag = frag_,
 .color = { { .format = colorFormat } },
 .depthFormat = depthFormat,
 .cullMode = lvk::CullMode_None,
 .samplesCount = numSamples,
 .minSampleShading = numSamples>1 ? 0.25f : 0 });
 pipelineWireframe_ = ctx->createRenderPipeline({
 .vertexInput = meshData.streams,
 .smVert = vert_,
 .smFrag = frag_,
 .color = { { .format = colorFormat } },
 .depthFormat = depthFormat,
 .cullMode = lvk::CullMode_None,
 .polygonMode = lvk::PolygonMode_Line,
 .samplesCount = numSamples });
 }


This approach to data storage makes it a useful building block for a rendering engine, where individual 
mesh instances can be added or removed dynamically at runtime. Now let’s take a look at how actual 
rendering works.

### How it works...

The actual rendering takes place in the VKMesh::draw() method, and it is fairly straightforward.
1. First, we bind the index and vertex buffers for rendering the entire scene, along with the 
appropriate rendering pipeline and the depth state:
 void draw(
 lvk::ICommandBuffer& buf,
 const mat4& view,
 const mat4& proj,
 TextureHandle texSkyboxIrradiance = {},
 bool wireframe = false) const
 {
 buf.cmdBindIndexBuffer(bufferIndices_, lvk::IndexFormat_UI32);
 buf.cmdBindVertexBuffer(0, bufferVertices_);
 buf.cmdBindRenderPipeline(
 wireframe ? pipelineWireframe_ : pipeline_);
 buf.cmdBindDepthState({
 .compareOp = lvk::CompareOp_Less,
 .isDepthWriteEnabled = true });
2. Next, we prepare the push constant data for our GLSL shaders. Unlike the Explosions example 
from the previous recipe, Using descriptor indexing and arrays of textures in Vulkan, this approach uses a single push constant value for the entire scene. This is done for simplicity and 
performance reasons, as we aim to render a scene with thousands of meshes in one draw call:
 const struct {
 mat4 viewProj;
 uint64_t bufferTransforms;
 uint64_t bufferDrawData;
 uint64_t bufferMaterials;
 uint32_t texSkyboxIrradiance;
 } pc = {
 .viewProj = proj * view,
 .bufferTransforms = ctx->gpuAddress(bufferTransforms_),
 .bufferDrawData = ctx->gpuAddress(bufferDrawData_),
 .bufferMaterials = ctx->gpuAddress(bufferMaterials_),
 .texSkyboxIrradiance = texSkyboxIrradiance.index(),
 };
 buf.cmdPushConstants(pc);


3. Rendering can now be done in a single draw call, vkCmdDrawIndexedIndirect(). We can 
render an entire scene with thousands of meshes, each using a distinct material, all with just 
one indirect draw call:
 buf.cmdDrawIndexedIndirect(
 bufferIndirect_, 0, numMeshes_);
 }
4. A few more important things to note here. Since we want our scene to be dynamic, we 
might need to update the model-to-world transformations every frame. The member function updateGlobalTransforms() takes a vector of mat4 matrices and uploads it to the 
bufferTransforms_ buffer:
 void updateGlobalTransforms(
 const mat4* data, size_t numMatrices) const
 {
 ctx->upload(bufferTransforms_, data, numMatrices * sizeof(mat4));
 }
5. Similarly, with materials, we may need to update a material occasionally. The 
updateMaterialIndex() function is a member function designed to handle this. We will 
demonstrate how to use it in the next recipe:
 void updateMaterial(
 const Material* materials,
 int updateMaterialIndex) const
 {
 if (updateMaterialIndex < 0) return;
 const GLTFMaterialDataGPU mat =
 convertToGPUMaterial(ctx,
 materials[updateMaterialIndex],
 textureFiles_, textureCache_);
 ctx->upload(bufferMaterials_, &mat, sizeof(mat),
 sizeof(mat) * updateMaterialIndex);
 }
};
With the helper class VKMesh in place, we now have the foundation to render our scene data using 
Vulkan and LightweightVK. This class handles all the complexities of managing buffers, materials, 
transformations, and indirect draw commands, allowing us to efficiently render an entire scene. Now 
that we’ve covered the setup, let’s move on to the next recipe, where we will see how all of this comes 
together in our first complete scene rendering application.


## Putting it all together into a scene editing application

In the previous recipes, we introduced a significant amount of scaffolding code to manage the scene 
hierarchy, handle materials, and prepare GPU buffers for rendering. Now, let’s put all of that together 
by implementing a complete scene-rendering application based on those code fragments. To make 
the application even more engaging, we will add some scene graph editing capabilities, allowing us 
to interactively modify and update the scene in real time.

### Getting ready

Before proceeding with this recipe, be sure to revisit the recipe Implementing indirect rendering with 
Vulkan along with all the related recipes to review the data structures we use for storing and managing our scene. This will ensure you have a solid understanding of how everything fits together before 
diving into the next steps.
The source code for this recipe is located at Chapter08/02_SceneGraph.

### How to do it...

Let’s go through the C++ part of the application Chapter08/02_SceneGraph/src/main.cpp, starting 
from the main() function.
1. First, we load and precache our scene data using the method described earlier in this chapter 
in the recipe Loading and saving a scene graph:
const char* fileNameCachedMeshes = ".cache/ch08_orrery.meshes";
const char* fileNameCachedMaterials = ".cache/ch08_orrery.materials";
const char* fileNameCachedHierarchy = ".cache/ch08_orrery.scene";
int main() {
 if (!isMeshDataValid(fileNameCachedMeshes)||
 !isMeshHierarchyValid(fileNameCachedHierarchy)||
 !isMeshMaterialsValid(fileNameCachedMaterials))
 {
 printf("No cached data found. Precaching...\n\n");
 MeshData meshData;
 Scene ourScene;
 loadMeshFile("data/meshes/orrery/scene.gltf",
 meshData, ourScene, true);
 saveMeshData(fileNameCachedMeshes, meshData);
 saveMeshDataMaterials(fileNameCachedMaterials, meshData);
 saveScene(fileNameCachedHierarchy, ourScene);
 }


2. Next, we load the precached scene, which significantly speeds up subsequent runs of our 
application after the initial caching is complete. This will be especially important in the final 
recipe of this chapter, where we load the complex Lumberyard Bistro mesh. For our VKMesh
class, we will need instances of MeshData, MeshFileHeader, and Scene:
 MeshData meshData;
 const MeshFileHeader header =
 loadMeshData(fileNameCachedMeshes, meshData);
 loadMeshDataMaterials(fileNameCachedMaterials, meshData);
 Scene scene;
 loadScene(fileNameCachedHierarchy, scene);
3. Let’s create a VulkanApp and a 3D drawing canvas, following the steps outlined in the recipe 
Implementing immediate mode 3D drawing canvas in Chapter 4. We will use the canvas to render 
the bounding boxes of the meshes in our scene. The drawWireframe flag toggles the wireframe 
rendering mode, while selectedNode stores the ID of the node we want to edit in the UI. We 
will demonstrate how this works shortly:
 VulkanApp app({
 .initialCameraPos = vec3(-0.88f, 1.26f, 1.07f),
 .initialCameraTarget = vec3(0, -0.6f, 0),
 });
 LineCanvas3D canvas3d;
 unique_ptr<lvk::IContext> ctx(app.ctx_.get());
 bool drawWireframe = false;
 int selectedNode = -1;
4. Before entering the rendering loop, let’s create our VKMesh instance using the loaded scene 
data and the depth buffer format obtained from VulkanApp. Since there’s only one rendering 
pass, the setup for RenderPass and FrameBuffer is trivial:
 const VKMesh mesh(ctx, meshData, scene,
 ctx->getSwapchainFormat(),
 app.getDepthFormat());
 app.run([&](uint32_t width, uint32_t height,
 float aspectRatio, float deltaSeconds)
 {
 const lvk::RenderPass renderPass = {
 .color = {
 { .loadOp = lvk::LoadOp_Clear,
 .clearColor = { 1.f, 1.f, 1.f, 1.f } } },
 .depth = { .loadOp = lvk::LoadOp_Clear,
 .clearDepth = 1.f }


 };
 const lvk::Framebuffer framebuffer = {
 .color = {{.texture = ctx->getCurrentSwapchainTexture()}},
 .depthStencil = { .texture = app.getDepthTexture() },
 };
5. The view and projection matrices remain unchanged for the entire frame. The 
updateMaterialIndex variable is used to update a single material from the UI:
 const mat4 proj = glm::perspective(45.0f,
 aspectRatio, 0.01f, 100.0f);
 const mat4 view = app.camera_.getViewMatrix();
 int updateMaterialIndex = -1;
6. Now, let’s render the scene. All the work has already been done in VKMesh::draw() from the 
previous recipe, Implementing indirect rendering with Vulkan. We simply need to call it here 
with the appropriate parameters:
 ICommandBuffer& buf = ctx->acquireCommandBuffer();
 buf.cmdBeginRendering(renderPass, framebuffer);
 buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
 mesh.draw(buf, view, proj, {}, drawWireframe);
 buf.cmdPopDebugGroupLabel();
7. On top of the mesh, we render a grid, as explained in the recipe Implementing an infinite grid 
GLSL shader in Chapter 5. This is accompanied by an FPS counter and a default keyboard 
helpers memo.
 app.drawGrid(buf, proj, vec3(0, -1.0f, 0));
 app.imgui_->beginFrame(framebuffer);
 app.drawFPS();
 app.drawMemo();
8. Next, we render the bounding boxes for all meshes in our scene using the 3D canvas from the 
recipe Implementing immediate mode 3D drawing canvas in Chapter 4:
 canvas3d.clear();
 canvas3d.setMatrix(proj * view);
 for (auto& p : scene.meshForNode) {
 const BoundingBox box = meshData.boxes[p.second];
 canvas3d.box(scene.globalTransform[p.first],
 box, vec4(1, 0, 0, 1));
 }


9. The extra step is to draw the scene editing UI itself using the ImGui library. The UI displays 
the entire scene graph tree with all the nodes and a checkbox to toggle the wireframe mode 
for each node. 
 const ImGuiViewport* v = ImGui::GetMainViewport();
 ImGui::SetNextWindowPos(ImVec2(10, 200));
 ImGui::SetNextWindowSize(
 ImVec2(v->WorkSize.x / 6,
 v->WorkSize.y - 210));
 ImGui::Begin("Scene graph", nullptr,
 ImGuiWindowFlags_NoFocusOnAppearing |
 ImGuiWindowFlags_NoCollapse |
 ImGuiWindowFlags_NoResize);
 ImGui::Checkbox("Draw wireframe", &drawWireframe);
 ImGui::Separator();
10. The scene graph tree is rendered recursively using the renderSceneTreeUI() function, which 
we’ll examine in more detail shortly, in the How it works... section. Our UI allows selecting a 
scene node by clicking on a node name in the tree. Once a node is selected, a new UI window 
appears, enabling the editing of the selected node’s properties. This functionality is handled 
by the editNodeUI() function, which we will explore shortly:
 const int node = renderSceneTreeUI(scene, 0, selectedNode);
 if (node > -1) selectedNode = node;
 ImGui::End();
 editNodeUI(scene, meshData, view, proj,
 selectedNode, updateMaterialIndex,
 mesh.textureCache_);
11. If a node is selected, we render its bounding box in green. After that, we can finalize both the 
3D canvas and ImGui rendering, and then submit our command buffer:
 if (selectedNode > -1 && scene.hierarchy[
 selectedNode].firstChild < 0)
 {
 const uint32_t meshId = scene.meshForNode[selectedNode];
 const BoundingBox box = meshData.boxes[meshId];
 canvas3d.box(
 scene.globalTransform[selectedNode],
 box, vec4(0, 1, 0, 1));
 }
 canvas3d.render(*ctx.get(), framebuffer, buf);
 app.imgui_->endFrame(buf);


 buf.cmdEndRendering();
 ctx->submit(buf, ctx->getCurrentSwapchainTexture());
12. After rendering, we need to update the model-to-world transformation matrices, as described 
earlier in the recipe Implementing transformation trees. If any transformations were changed, 
we must update the GPU buffer by calling VKMesh::updateGlobalTransforms(), which we 
implemented in the recipe Implementing indirect rendering with Vulkan:
 if (recalculateGlobalTransforms(scene))
 mesh.updateGlobalTransforms(
 scene.globalTransform.data(),
 scene.globalTransform.size());
13. The same applies to materials. If a material has been modified through the UI, we need to 
update the GPU buffer data for that material:
 if (updateMaterialIndex > -1)
 mesh.updateMaterial(meshData.materials.data(),
 updateMaterialIndex);
 });
 ctx.release();
 return 0;
}
This concludes the high-level flow of our sample application. Now, let’s dive deeper into how the scene 
editing helper functions work. We’ll explore the details of the functions renderSceneTreeUI() and 
editNodeUI(), as well as how they interact with the rest of the application to allow us to manipulate 
and edit the scene in real time.

### How it works...

One helper function is renderSceneTreeUI(), which recursively renders the scene graph. This function 
is central to rendering the scene graph UI, so let’s take a look at it in detail.
Note
Here’s the getNodeName() helper function for your convenience:
std::string getNodeName(const Scene& scene, int node) {
 const int strID = scene.nameForNode.contains(node) ?
 scene.nameForNode.at(node) : -1;
 return strID > -1 ?
 scene.nodeNames[strID] : std::string();
}


1. The function takes three arguments: the Scene we want to render, the ID of the node we want 
to display in the UI, and the ID of the selected node. Since the UI includes node names, we can 
use getNodeName() from shared/Scene/Scene.h to retrieve the names of the nodes:
int renderSceneTreeUI(
 const Scene& scene, int node, int selectedNode)
{
 const std::string name = getNodeName(scene, node);
 const std::string label = name.empty() ?
 (std::string("Node") + std::to_string(node)) :
 name;
2. Depending on whether the scene node is a leaf node – meaning it has no child nodes – we set
different colors and ImGui tree node flags. This setup enables ImGui to “unfold” a non-leaf 
tree node when you click on the bullet next to its name:
 const bool isLeaf = scene.hierarchy[node].firstChild < 0;
 ImGuiTreeNodeFlags flags =
 isLeaf ? ImGuiTreeNodeFlags_Leaf |
 ImGuiTreeNodeFlags_Bullet : 0;
 if (node == selectedNode)
 flags |= ImGuiTreeNodeFlags_Selected;
 ImVec4 color = isLeaf ?
 ImVec4(0, 1, 0, 1) : ImVec4(1, 1, 1, 1);
3. Certain nodes, like the root node and a few others, should always be open by default. Let’s 
update the ImGui flags for those nodes accordingly:
 if (name.starts_with("Root")) {
 flags |= ImGuiTreeNodeFlags_DefaultOpen |
 ImGuiTreeNodeFlags_Leaf |
 ImGuiTreeNodeFlags_Bullet;
 color = ImVec4(0.9f, 0.6f, 0.6f, 1);
 }
 if (name == "sun" || name == "sun_0" ||
 name.ends_with(".stk") ||
 name.starts_with("p3.earth")) {
 flags |= ImGuiTreeNodeFlags_DefaultOpen;
 }


4. Now, we can apply the appropriate color and call ImGui::TreeNodeEx() to handle the actual 
rendering of the tree UI:
 ImGui::PushStyleColor(ImGuiCol_Text, color);
 const bool isOpened = ImGui::TreeNodeEx(
 &scene.hierarchy[node], flags, "%s", label.c_str());
 ImGui::PopStyleColor();
 ImGui::PushID(node);
5. If we click on a leaf node that represents a mesh, we mark it as the selected node. If a non-leaf 
node is opened, we recursively render the UI for all of its children. Here, we leverage our First 
Child, Next Sibling scene graph representation by starting with the first child and traversing 
through all its sibling nodes:
 if (ImGui::IsItemHovered() && isLeaf)
 selectedNode = node;
 if (isOpened) {
 for (int ch = scene.hierarchy[node].firstChild;
 ch != -1;
 ch = scene.hierarchy[ch].nextSibling)
 {
 if (int subNode =
 renderSceneTreeUI(scene, ch, selectedNode);
 subNode>-1) selectedNode = subNode;
 }
 ImGui::TreePop();
 }
 ImGui::PopID();
 return selectedNode;
}
This function serves as a central building block for creating custom scene editing UIs of any kind. You 
can use it as a foundation to render more complex UIs tailored to your needs. Here is a screenshot 
showing what the scene tree UI looks like:


Figure 8.4: The scene tree user interface
Now, let’s move on to explore another helper function, editNodeUI(), which offers similar foundational 
tools for creating an editing interface.
1. The editNodeUI() function takes additional parameters, as it requires access to MeshData and 
textures. The view and projection matrices are also needed to render transformation gizmos 
on top of the mesh within the window:
void editNodeUI(Scene& scene, MeshData& meshData,
 const mat4& view, const mat4 proj,
 int node,
 int& outUpdateMaterialIndex,
 const TextureCache& textureCache)
{
2. We use the ImGuizmo library to render transformation gizmos, which provide an intuitive 
way for users to manipulate the position, rotation, and scale of objects directly in the scene. 
ImGuizmo allows us to display interactive 3D handles, such as arrows for translation and circles 
for rotation, making it easier for users to visualize and control transformations in real time. 
You can check it out at https://github.com/CedricGuillemet/ImGuizmo:


 ImGuizmo::SetOrthographic(false);
 ImGuizmo::BeginFrame();
 std::string name = getNodeName(scene, node);
 std::string label = name.empty() ?
 (std::string("Node") + std::to_string(node)) :
 name;
 label = "Node: " + label;
3. We draw the editor window on the right side of the screen:
 if (const ImGuiViewport* v = ImGui::GetMainViewport()) {
 ImGui::SetNextWindowPos(ImVec2(v->WorkSize.x * 0.83f, 200));
 ImGui::SetNextWindowSize(
 ImVec2(v->WorkSize.x / 6,
 v->WorkSize.y - 210));
 }
 ImGui::Begin("Editor", nullptr,
 ImGuiWindowFlags_NoFocusOnAppearing |
 ImGuiWindowFlags_NoCollapse |
 ImGuiWindowFlags_NoResize);
 if (!name.empty())
 ImGui::Text("%s", label.c_str());
 if (node >= 0) {
 ImGui::Separator();
 ImGuizmo::SetID(1);
4. In our editing window, we first retrieve the global and local node transformations from the 
scene. Then, we call the editTransformUI() helper function to edit the model-to-world transformation of the selected node. We will take a closer look at how this function works shortly:
 mat4 globalTransform = scene.globalTransform[node];
 mat4 srcTransform = globalTransform;
 mat4 localTransform = scene.localTransform[node];
 if (editTransformUI(view, proj, globalTransform))
 {
5. If the transformation was edited, we need to calculate the delta value, taking into account the 
node’s global transform, and then modify the local transform in the scene accordingly. This 
ensures that the changes are properly applied and that the node’s transformation remains 
consistent within the scene's coordinate system. The markAsChanged() function triggers an 
update to the scene tree, as explained in the recipe Implementing transformation trees:
 mat4 deltaTransform = glm::inverse(srcTransform)*globalTransform;
 scene.localTransform[node] = localTransform * deltaTransform;
 markAsChanged(scene, node);
 }


6. The second part of our scene editor is the material editor. We call the editMaterialUI() function to handle the material editing process. We will dive into the details of this function shortly:
 ImGui::Separator();
 ImGui::Text("%s", "Material");
 editMaterialUI(scene, meshData, node,
 outUpdateMaterialIndex, textureCache);
 }
 ImGui::End();
}
The UI of the node editor appears as shown in the following screenshot. In this case, we have selected 
a node named sun_0_Mesh_0.
Figure 8.5: The node editor user interface
Let’s take a closer look at the editMaterialUI() function, which is responsible for rendering the 
material section of the UI shown in this screenshot.


1. The editMaterialUI() function accepts the Scene, MeshData, a node ID, a reference to output the index of the modified material, and the texture cache. To simplify the texture editing 
process, we introduce a global variable called textureToEdit. This allows us to open a modal 
window, making it easier to replace textures in the material:
int* textureToEdit = nullptr;
bool editMaterialUI(
 Scene& scene, MeshData& meshData,
 int node, int& outUpdateMaterialIndex,
 const TextureCache& textureCache)
{
 if (!scene.materialForNode.contains(node))
 return false;
 const uint32_t matIdx = scene.materialForNode[node];
 Material& material = meshData.materials[matIdx];
2. We introduce a Boolean flag, updated, to indicate if any of the material properties have been 
updated. Then, we use ImGui functions to edit individual properties, such as emissiveFactor
or baseColorFactor. You can easily extend this system to include additional properties of your 
choice by adding more relevant ImGui controls:
 bool updated = false;
 updated |= ImGui::ColorEdit3("Emissive color",
 glm::value_ptr(material.emissiveFactor));
 updated |= ImGui::ColorEdit3("Base color",
 glm::value_ptr(material.baseColorFactor));
3. Let’s add one more cool editing trick: the ability to replace a texture in a material by simply 
clicking on it. To achieve this, we implement a helper lambda function that opens a modal 
popup when a texture image is clicked: 
 const char* ImagesGalleryName = "Images Gallery";
 auto drawTextureUI =
 [&textureCache, ImagesGalleryName](
 const char* name, int& texture)
 {
 if (texture == -1) return;
 ImGui::Text(name);
 ImGui::Image(textureCache[texture].index(),
 ImVec2(512, 512), ImVec2(0, 1), ImVec2(1, 0));
 if (ImGui::IsItemClicked()) {
 textureToEdit = &texture;
 ImGui::OpenPopup(ImagesGalleryName);
 }
 };


4. Now, we can render all the material textures using this lambda function:
 drawTextureUI("Base texture:", material.baseColorTexture);
 drawTextureUI("Emissive texture:", material.emissiveTexture);
 drawTextureUI("Normal texture:", material.normalTexture);
 drawTextureUI("Opacity texture:", material.opacityTexture);
5. Once all the material textures are rendered in the UI, we display a modal pop-up window 
featuring a gallery of all the loaded textures, allowing the user to easily pick and replace a 
texture. The modal pop-up window is centered on the screen:
 if (const ImGuiViewport* v = ImGui::GetMainViewport())
 {
 ImGui::SetNextWindowPos(
 ImVec2(v->WorkSize.x * 0.5f,
 v->WorkSize.y * 0.5f),
 ImGuiCond_Always, ImVec2(0.5f, 0.5f));
 }
6. The Images Gallery displays all the loaded textures arranged in a 4x4 grid: 
 if (ImGui::BeginPopupModal(
 ImagesGalleryName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
 {
 for (int i = 0; i != textureCache.size(); i++) {
 if (i && i % 4 != 0) ImGui::SameLine();
 ImGui::Image(textureCache[i].index(),
 ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));
7. When a texture is hovered over, we add a transparent color overlay to visually highlight the 
selected texture. The color value 0x66ffffff represents a semi-transparent white:
 if (ImGui::IsItemHovered()) {
 ImGui::GetWindowDrawList()->AddRectFilled(
 ImGui::GetItemRectMin(),
 ImGui::GetItemRectMax(),
 0x66ffffff);
 }
8. When a texture is clicked, we assign its index to our textureToEdit global variable, set the 
updated flag, and then close the pop-up window:
 if (ImGui::IsItemClicked()) {
 *textureToEdit = i;
 updated = true;
 ImGui::CloseCurrentPopup();
 }


 }
 ImGui::EndPopup();
 }
9. If any of the material properties were updated, the function outputs the index of the updated 
material:
 if (updated) outUpdateMaterialIndex = matIdx;
 return updated;
}
The Images Gallery rendered by this function appears as shown in the following screenshot. It displays 
all the loaded textures in a grid layout, allowing easy selection and replacement of textures.
Figure 8.6: The Images Gallery user interface


That was the complete C++ code for this application. The GLSL shaders will be covered in the last 
recipe, Rendering large scenes.
The running application should render the following image:
Figure 8.7: The scene graph application
Try clicking on different scene nodes in the scene tree on the right and editing their material properties
and transformations using the 3D gizmos.
Now, let’s jump to the next recipe to learn some advanced scene graph manipulations.
There’s more...
The scene editing methods provided in this recipe serve as basic building blocks for creating a 3D scene 
editing user interface. To build more advanced UIs, we recommend exploring additional features of 
the ImGuizmo library https://github.com/CedricGuillemet/ImGuizmo.

## Deleting nodes and merging scene graphs

Our scene graph management routines, as described earlier in this chapter, are incomplete without 
a few additional operations:
- Deleting scene nodes
- Merging multiple scenes into one (in our case, combining the exterior and interior objects of 
the Bistro scene)
- Merging material and mesh data lists
- Merging multiple static meshes with the same material into a single, larger mesh


These operations are crucial for the final demo application of this chapter. In the original Lumberyard Bistro scene, a large tree in the backyard contains thousands of small orange and green leaves, 
which represent nearly two-thirds of the total draw call count in the scene – approximately 18'000
out of 27'000 objects.
This recipe describes the deleteSceneNodes() and mergeNodesWithMaterial() routines, which are 
used for manipulating the scene graph. Along with the next recipe Rendering large scenes, these functions complete our scene graph data management.

### Getting ready

Let’s recall the recipe Using data-oriented design for a scene graph, where we packed all the scene data 
into continuous arrays wrapped in std::vector for convenience, making them directly usable by 
the GPU. In this recipe, we use STL’s partitioning algorithms to keep everything tightly packed while 
efficiently deleting and merging scene nodes.
To start, let’s implement a utility function to delete a collection of items from an array. While it may 
seem unnecessary to have a routine that removes a collection of nodes all at once, even in the simplest 
case of deleting a single node, we must also ensure that all of its child nodes are properly removed.
The idea behind this approach is to use the std::stable_partition() algorithm to move all nodes 
marked for deletion to the end of the array. Once the nodes to be deleted are relocated, we can simply 
resize the container to remove them from the active scene. This method ensures that we maintain 
the relative order of the nodes that are not deleted, while efficiently cleaning up those that are. The 
following diagram illustrates the deletion process:
Figure 8.8: Deleting nodes from a scene graph
On the left, we see the original scene graph, where nodes ch1, ch4, ch5, and ch6 are highlighted for
deletion (dark red). In the middle, the nodes are arranged in a linear list, with arrows showing the 
references between them. On the right, the nodes have been reorganized after partitioning, and the 
updated scene graph is displayed.


### How to do it...

Let’s explore how to implement this functionality.
1. The eraseSelected() routine from shared/Utils.h is designed to be flexible enough to delete 
any type of item from an array, so we use template arguments for its implementation:
template <typename T, typename Index = int>
void eraseSelected(
 std::vector<T>& v, const std::vector<Index>& selection)
{
 v.resize( std::distance(v.begin(),
 std::stable_partition(v.begin(), v.end(),
 [&selection, &v](const T& item) {
 return !std::binary_search(
 selection.begin(), selection.end(),
 static_cast<Index>(
 static_cast<const T*>(&item) - &v[0]));
 })) );
}
The function “chops off” the elements moved to the end of the vector by using resize(). The 
number of items to retain is determined by calculating the distance from the start of the array 
to the iterator returned by the partition function. The std::stable_partition() algorithm 
takes a lambda function that determines whether an element should be moved to the end of 
the array. In this case, the lambda checks if the item is in the selection container passed as an 
argument. While the typical way to find an item’s index in an array is by using std::distance()
and std::find(), we can also rely on the more straightforward pointer arithmetic, since the 
container is tightly packed.
2. Now that we have our workhorse, the eraseSelected() routine, we can implement scene node 
deletion. When deleting a node, all of its children must also be marked for deletion. We achieve 
this by using a recursive routine that iterates over all the child nodes, adding each node’s index 
to the deletion array. This function is located in shared/Scene/Scene.cpp:
void collectNodesToDelete(
 const Scene& scene, int node,
 std::vector<uint32_t>& nodes)
{
 for (int n = scene.hierarchy[node].firstChild;
 n != -1;
 n = scene.hierarchy[n].nextSibling) {
 addUniqueIdx(nodes, n);
 collectNodesToDelete(scene, n, nodes);
 }
}


3. A helper function addUniqueIndex() ensures that items are not added multiple times: 
void addUniqueIdx(
 std::vector<uint32_t>& v, uint32_t index)
{
 if (!std::binary_search(v.begin(), v.end(), index))
 v.push_back(index);
}
One subtle requirement here is that the array must be sorted. When all the children follow 
their parents in order, this is not an issue. However, if that’s not the case, std::find() must 
be used, which naturally increases the runtime cost of the algorithm.
4. Our deletion routine, deleteSceneNodes(), begins by adding all child nodes to the deleted 
nodes container. To keep track of the moved nodes, we create an array of node indices starting from zero. The std::iota() algorithm fills the range with sequentially increasing values, 
starting with the specified value 0:
void deleteSceneNodes(
 Scene& scene,
 const std::vector<uint32_t>& nodesToDelete)
{
 auto indicesToDelete = nodesToDelete;
 for (uint32_t i : indicesToDelete)
 collectNodesToDelete(scene, i, indicesToDelete);
 std::vector<int> nodes(scene.hierarchy.size());
 std::iota(nodes.begin(), nodes.end(), 0);
5. After that, we store the original node count and remove all the indices from our linear index 
list. To adjust the child node indices, we generate a linear mapping table that maps the old 
node indices to the new ones:
 const size_t oldSize = nodes.size();
 eraseSelected(nodes, indicesToDelete);
 std::vector<int> newIndices(oldSize, -1);
 for (int i = 0; i < nodes.size(); i++)
 newIndices[nodes[i]] = i;
6. Before deleting nodes from the Hierarchy array, we first remap all node indices. The following 
lambda function modifies each Hierarchy item by locating the corresponding non-null node 
in the newIndices container:
 auto nodeMover =
 [&scene, &newIndices](Hierarchy& h)
 {
 return Hierarchy{


 .parent = (h.parent != -1) ? newIndices[h.parent] : -1,
 .firstChild = findLastNonDeletedItem(scene, newIndices, h.firstChild),
 .nextSibling = findLastNonDeletedItem(scene, newIndices, h.nextSibling),
 .lastSibling = findLastNonDeletedItem(scene, newIndices, h.lastSibling),
 };
 };
7. The std::transform() algorithm updates all the nodes in the hierarchy. Once the node indices 
are fixed, we’re ready to delete the data. Three calls to eraseSelected() remove the unused 
hierarchy and transform items:
 std::transform(scene.hierarchy.begin(),
 scene.hierarchy.end(),
 scene.hierarchy.begin(),
 nodeMover);
 eraseSelected(scene.hierarchy, indicesToDelete);
 eraseSelected(scene.localTransform, indicesToDelete);
 eraseSelected(scene.globalTransform, indicesToDelete);
8. Finally, we need to adjust the indices in the mesh, material, and name maps. To do this, we 
use the shiftMapIndices() function, which is described below:
 shiftMapIndices(scene.meshForNode, newIndices);
 shiftMapIndices(scene.materialForNode, newIndices);
 shiftMapIndices(scene.nameForNode, newIndices);
}
9. The search for node replacement, used during node index shifting, is implemented recursively. 
The findLastNonDeletedItem() function returns the index of a non-deleted node replacement:
int findLastNonDeletedItem(const Scene& scene,
 const std::vector<int>& newIndices, int node)
{
 if (node == -1) return -1;
 return newIndices[node] == -1 ?
 findLastNonDeletedItem(scene, newIndices,
 scene.hierarchy[node].nextSibling) :
 newIndices[node];
}
If the input is empty, no replacement is necessary. If no replacement is found for the node, we 
recursively move to the next sibling of the deleted node.


10. The final function, shiftMapIndices(), updates the pair::second value in each item in the 
map:
void shiftMapIndices(
 std::unordered_map<uint32_t, uint32_t>& items,
 const std::vector<int>& newIndices)
{
 std::unordered_map<uint32_t, uint32_t> newItems;
 for (const auto& m : items) {
 int newIndex = newIndices[m.first];
 if (newIndex != -1)
 newItems[newIndex] = m.second;
 }
 items = newItems;
}
The deleteSceneNodes() routine helps us compress and optimize the scene graph, while also merging 
multiple meshes with the same material. Now, we need a method to combine multiple meshes into 
a single one and remove any scene nodes referring to the merged meshes. The process of merging 
mesh data mainly involves modifying the index data. Let’s go through the steps:
1. The mergeNodesWithMaterial() function relies on two helper functions. The first one calculates the total number of merged indices. We keep track of the starting vertex offset for each 
mesh. Then, in a loop, we shift the indices within the individual mesh blocks of the meshData.
indexData array. Additionally, for each Mesh object, a new minVtxOffset value is assigned 
to the vertex data offset field. The function returns the difference between the original and 
merged index counts, which also represents the offset where the merged index data begins:
uint32_t shiftMeshIndices(
 MeshData& meshData,
 const vector<uint32_t>& meshesToMerge)
{
 uint32_t minVtxOffset = std::numeric_limits<uint32_t>::max();
 for (uint32_t i : meshesToMerge) {
 minVtxOffset = std::min(
 meshData.meshes[i].vertexOffset, minVtxOffset);
 }
 uint32_t mergeCount = 0;
 for (uint32_t i : meshesToMerge) {
 Mesh& m = meshData.meshes[i];
 const uint32_t delta = m.vertexOffset - minVtxOffset;
 const uint32_t idxCount = m.getLODIndicesCount(0);
 for (uint32_t ii = 0; ii < idxCount; ii++) {
 meshData.indexData[m.indexOffset + ii] += delta;


 }
 m.vertexOffset = minVtxOffset;
 mergeCount += idxCount;
 }
 return meshData.indexData.size() - mergeCount;
}
2. The mergeIndexArray() function copies the indices from each mesh into a new, consolidated 
indices array: 
void mergeIndexArray(MeshData& md,
 const vector<uint32_t>& meshesToMerge,
 unordered_map<uint32_t, uint32_t>& oldToNew)
{
 vector<uint32_t> newIndices(md.indexData.size());
 uint32_t copyOffset = 0;
 uint32_t mergeOffset = shiftMeshIndices(md, meshesToMerge);
For each mesh, we determine where to copy its index data. The copyOffset value is used for 
meshes that are not merged, while the mergeOffset value starts at the beginning of the merged 
index data returned by the shiftMeshIndices() function.
3. Two variables store the mesh indices of the merged mesh and the copied mesh. We iterate 
through all the meshes to determine if the current one should be merged:
 const size_t mergedMeshIndex = md.meshes.size() - meshesToMerge.size();
 uint32_t newIndex = 0u;
 for (auto midx = 0u; midx < md.meshes.size(); midx++) {
 const bool shouldMerge = std::binary_search(
 meshesToMerge.begin(), meshesToMerge.end(), midx);
4. Each index is recorded in the old-to-new correspondence map:
 oldToNew[midx] = shouldMerge ?
 mergedMeshIndex : newIndex; newIndex += shouldMerge ? 0 : 1;
5. The offset of the index block for this mesh is adjusted by first calculating the source offset for 
the index data:
 Mesh& mesh = md.meshes[midx];
 const uint32_t idxCount = mesh.getLODIndicesCount(0);
 const auto start = md.indexData.begin() + mesh.indexOffset;
 mesh.indexOffset = copyOffset;


6. We select between the two offsets and copy the index data from the original array to the new 
one. The updated index array is then integrated into the Mesh data structure:
 uint32_t* const offsetPtr =
 shouldMerge ? &mergeOffset : &copyOffset;
 std::copy(start, start + idxCount,
 newIndices.begin() + *offsetPtr);
 *offsetPtr += idxCount;
 }
 md.indexData = newIndices;
7. The final step in the merge process is the creation of the merged mesh. We copy the mesh 
descriptor of the first of the merged meshes and assign the new LOD offsets:
 Mesh lastMesh = md.meshes[meshesToMerge[0]];
 lastMesh.indexOffset = copyOffset;
 lastMesh.lodOffset[0] = copyOffset;
 lastMesh.lodOffset[1] = mergeOffset;
 lastMesh.lodCount = 1;
 md.meshes.push_back(lastMesh);
}
The mergeNodesWithMaterial() routine omits a few key details. First, we only merge the finest LOD. 
This is sufficient for our purposes, as our scene consists of many simple meshes (1-2 triangles) with only 
a single LOD. Second, we assume that all merged meshes have the same transformation. This works 
for our test scene, but if proper transformations are needed, each vertex should be transformed into 
the global coordinate system and then back into the local coordinates of the node where the merged 
mesh is placed. Let’s explore the implementation:
1. To avoid string comparisons, we convert material names into their corresponding indices in 
the material name array:
void mergeNodesWithMaterial(Scene& scene,
 MeshData& meshData,
 const std::string& materialName)
{
 const int oldMaterial = (int)std::distance(
 std::begin(scene.materialNames),
 std::find(std::begin(scene.materialNames),
 std::end(scene.materialNames),
 materialName));


2. Once you have the material index, gather all the scene nodes that need to be deleted:
 std::vector<uint32_t> toDelete;
 for (size_t i = 0u;
 i < scene.hierarchy.size(); i++) {
 if (scene.meshForNode.contains(i) &&
 scene.materialForNode.contains(i) &&
 (scene.materialForNode.at(i)==oldMaterial))
 toDelete.push_back(i);
 }
3. The number of meshes to merge matches the number of scene nodes marked for deletion (at 
least in our case), so convert the scene node indices into mesh indices:
 vector<uint32_t> meshesToMerge(toDelete.size());
 std::transform(
 toDelete.begin(), toDelete.end(),
 meshesToMerge.begin(),
 [&scene](uint32_t i)
 { return scene.meshForNode.at(i); });
4. A key part of this code merges the index data and assigns the updated mesh indices to the 
scene nodes:
 std::unordered_map<uint32_t, uint32_t> oldToNew;
 mergeIndexArray(meshData, meshesToMerge, oldToNew);
 for (auto& n : scene.meshForNode)
 n.second = oldToNew[n.second];
5. Finally, remove the individual merged meshes and attach a new node containing the merged 
meshes to the scene graph:
 int newNode = addNode(scene, 0, 1);
 scene.meshForNode[newNode] = (int)meshData.meshes.size() - 1;
 scene.materialForNode[newNode] = (uint32_t)oldMaterial;
 deleteSceneNodes(scene, toDelete);
}
The mergeNodesWithMaterial() function is used in the next recipe, Rendering large scenes, to merge 
multiple meshes in the Lumberyard Bistro scene.
Now, let’s jump to the next recipe to learn how to render a much more complex 3D model where these 
techniques will come in very handy.


## Rendering large scenes

In the previous recipes, we learned how to manipulate scene graphs and render a scene with some 
editing capabilities added to it. Now, let’s move on to rendering a more complex scene, Lumberyard 
Bistro, along with all its materials. All the scene storage and optimization techniques we’ve covered in 
this chapter will really come into play, especially given the large number of objects we need to handle 
in this 3D scene. Let’s dive deeper and take a closer look.

### Getting ready

Make sure to revisit the previous recipe, Putting it all together into a scene editing application, to refresh 
how our scene rendering is done.
The source code for this recipe is located in Chapter08/03_LargeScene.

### How to do it...

The rendering in this recipe is quite similar to the previous one, with a few key differences required 
to render the Bistro scene. The entire scene is made up of two separate mesh files: exterior.obj and 
interior.obj. We need to combine them into a single Scene object and then render them. Additionally, the scene includes some transparent objects, such as windows and bottles, which will require 
special handling.
Let’s begin by taking a look at the C++ code in Chapter08/03_LargeScene/src/main.cpp to understand 
the details.
1. The source code begins with the renderSceneTreeUI() function, which is very similar to the 
one from the previous recipe, Putting it all together into a scene editing application. The only 
difference here is the name of the scene nodes we want to keep open by default. The NewRoot
node is created when combining the two different scenes from exterior.obj and interior.
obj. We will delve into this further in a moment:
int renderSceneTreeUI(
 const Scene& scene, int node, int selectedNode)
{
 // …
 if (name == "NewRoot") {
 flags |= ImGuiTreeNodeFlags_DefaultOpen |
 ImGuiTreeNodeFlags_Leaf |
 ImGuiTreeNodeFlags_Bullet;
 color = ImVec4(0.9f, 0.6f, 0.6f, 1);
 }
 // …
}


2. Despite originating from two different .obj files, the scene is combined and cached into a 
single file, which makes subsequent loading almost instantaneous:
const char* fileNameCachedMeshes = ".cache/ch08_bistro.meshes";
const char* fileNameCachedMaterials = ".cache/ch08_bistro.materials";
const char* fileNameCachedHierarchy = ".cache/ch08_bistro.scene";
int main() {
 if (!isMeshDataValid(fileNameCachedMeshes) ||
 !isMeshHierarchyValid(fileNameCachedHierarchy)||
 !isMeshMaterialsValid(fileNameCachedMaterials))
 {
 printf("No cached mesh data found. Precaching...");
3. Here, we load two .obj files into two entirely separate sets of Scene and MeshData objects:
 MeshData meshData_Exterior;
 MeshData meshData_Interior;
 Scene ourScene_Exterior;
 Scene ourScene_Interior;
 loadMeshFile("deps/src/bistro/Exterior/exterior.obj",
 meshData_Exterior, ourScene_Exterior, false);
 loadMeshFile("deps/src/bistro/Interior/interior.obj",
 meshData_Interior, ourScene_Interior, false);
4. Before merging the two scenes into one, we perform some preprocessing. The mesh exterior.
obj contains a tree with over 10K individual leaves, each represented as a separate object. 
Rendering each leaf individually would be inefficient, so we merge them into larger submeshes for improved performance. The mergeNodesWithMaterial() function was covered in the 
previous recipe, Deleting nodes and merging scene graphs. It combines all mesh nodes that share 
a specified material name into a single large mesh:
 mergeNodesWithMaterial(
 ourScene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Orange_Leaves");
 mergeNodesWithMaterial(
 ourScene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Green_Leaves");
 mergeNodesWithMaterial(
 ourScene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Trunk");
5. Now that we have two optimized scenes, we can merge them into one large scene. To achieve this, 
we need a few helper functions: mergeScenes(), mergeMeshData(), and mergeMaterialLists(). 
We will take a closer look at each of these functions shortly:
 MeshData meshData;
 Scene ourScene;
 mergeScenes(ourScene,


 { &ourScene_Exterior, &ourScene_Interio, },
 {},
 { meshData_Exterior.meshes.size(),
 meshData_Interior.meshes.size() });
 mergeMeshData(meshData,
 { &meshData_Exterior, &meshData_Interior });
 mergeMaterialLists(
 { &meshData_Exterior.materials,
 &meshData_Interior.materials },
 { &meshData_Exterior.textureFiles,
 &meshData_Interior.textureFiles },
 meshData.materials, meshData.textureFiles);
6. Once the scene is merged, we want to scale the entire Bistro scene by 0.01 to fit it into our 
3D world bounds, recalculate its bounding boxes, and then save everything into our cached 
format, as explained in the recipe Loading and saving a scene graph:
 ourScene.localTransform[0] = glm::scale(vec3(0.01f));
 markAsChanged(ourScene, 0);
 recalculateBoundingBoxes(meshData);
 saveMeshData(fileNameCachedMeshes, meshData);
 saveMeshDataMaterials(
 fileNameCachedMaterials, meshData);
 saveScene(fileNameCachedHierarchy, ourScene);
 }
7. The loading code for cached scene data is the same as in the previous recipe:
 MeshData meshData;
 const MeshFileHeader header =
 loadMeshData(fileNameCachedMeshes, meshData);
 loadMeshDataMaterials(fileNameCachedMaterials, meshData);
 Scene scene;
 loadScene(fileNameCachedHierarchy, scene);
 VulkanApp app({
 .initialCameraPos = vec3(-19.26f, 8.47f,-7.32f),
Note
Because the Bistro mesh is quite large and initially stored in a textual .obj format, 
the first run to parse the .obj data can be extremely slow in Debug builds. We 
recommend running this demo app in the Release build for the first time. After 
the precaching is complete, subsequent runs in Debug mode will be very fast, 
enabling quick restarts even with this large mesh.


 .initialCameraTarget = vec3(0, 2.5f, 0),
 });
 LineCanvas3D canvas3d;
 unique_ptr<lvk::IContext> ctx(app.ctx_.get());
8. Next, let’s load skybox cube maps and GLSL shaders, and create a rendering pipeline:
 Holder<TextureHandle> texSkybox = loadTexture(ctx,
 "data/immenstadter_horn_2k_prefilter.ktx",
 TextureType_Cube);
 Holder<TextureHandle> texSkyboxIrradiance = loadTexture(ctx,
 "data/immenstadter_horn_2k_irradiance.ktx",
 TextureType_Cube);
 Holder<lvk::ShaderModuleHandle> vertSkybox = loadShaderModule(ctx,
 "Chapter08/02_SceneGraph/src/skybox.vert");
 Holder<ShaderModuleHandle> fragSkybox = loadShaderModule(ctx,
 "Chapter08/02_SceneGraph/src/skybox.frag");
 Holder<RenderPipelineHandle> pipelineSkybox =
 ctx->createRenderPipeline({
 .smVert = vertSkybox,
 .smFrag = fragSkybox,
 .color = { { .format = ctx->getSwapchainFormat() } },
 .depthFormat = app.getDepthFormat(),
 });
9. Node selection works similarly to the previous recipe. The only additional parameter is 
drawBoundingBoxes, which allows toggling the rendering of bounding boxes through the 
ImGui UI:
 bool drawWireframe = false;
 bool drawBoundingBoxes = false;
 int selectedNode = -1;
 const VKMesh mesh(ctx, meshData, scene,
 ctx->getSwapchainFormat(), app.getDepthFormat());
 app.run([&](uint32_t width, uint32_t height,
 float aspectRatio, float deltaSeconds) {
 const mat4 view = app.camera_.getViewMatrix();
 const mat4 proj = glm::perspective(
 45.0f, aspectRatio, 0.01f, 1000.0f);
10. The entire scene is rendered in a single render pass:
 const lvk::RenderPass renderPass = {
 .color = {
 { .loadOp = lvk::LoadOp_Clear,


 .clearColor = { 1.f, 1.f, 1.f, 1.f } } },
 .depth = { .loadOp = lvk::LoadOp_Clear,
 .clearDepth = 1.f } 
 };
 const lvk::Framebuffer framebuffer = {
 .color =
 { { .texture = ctx->getCurrentSwapchainTexture() } },
 .depthStencil =
 { .texture = app.getDepthTexture() },
 };
 ICommandBuffer& buf = ctx->acquireCommandBuffer();
 buf.cmdBeginRendering(renderPass, framebuffer);
11. Let’s render the skybox using its own rendering pipeline. Notice how the model-view-projection 
(mvp) matrix is constructed by discarding the translation part of the view matrix, ensuring the 
skybox always remains centered on the camera:
 buf.cmdBindRenderPipeline(pipelineSkybox);
 const struct {
 mat4 mvp;
 uint32_t texSkybox;
 } pc = {
 .mvp = proj * mat4(mat3(view)), 
 .texSkybox = texSkybox.index(),
 };
 buf.cmdPushConstants(pc);
 buf.cmdDraw(36);
12. Then, let’s render the Bistro scene. The entire mesh is rendered with just one indirect draw 
command. Here, we provide a sky irradiance texture to add some basic image-based lighting 
to our scene, enhancing the lighting in the environment. The remaining part of the C++ code 
is similar to the previous recipe. We render a grid and an FPS counter, draw the scene graph 
tree, and use our 3D canvas to render bounding boxes for the objects in the scene:
 mesh.draw(buf, view, proj, texSkyboxIrradiance, drawWireframe);
 app.imgui_->beginFrame(framebuffer);
 // …
 canvas3d.clear();
 canvas3d.setMatrix(proj * view);
 if (drawBoundingBoxes)
 for (auto& p : scene.meshForNode) {
 const BoundingBox box = meshData.boxes[p.second];
 canvas3d.box(scene.globalTransform[p.first],


 box, vec4(1, 0, 0, 1));
 }
 // … skipped the scene graph tree rendering here 
 canvas3d.render(*ctx.get(), framebuffer, buf);
 app.imgui_->endFrame(buf);
 buf.cmdEndRendering();
 ctx->submit(buf, ctx->getCurrentSwapchainTexture());
 });
 ctx.release();
 return 0;
}
Now let’s explore the GLSL shaders. They are shared with the previous demo application and located in Chapter08/02_SceneGraph/src/, with a common section of the code containing various 
buffer declarations used between the vertex and fragment shaders. This shared code can be found in 
Chapter08/02_SceneGraph/src/common.sp. Let’s take a closer look at it first.
1. The shared GLSL code reuses the common_material.sp material declarations, which were 
explained in Chapter 6. The DrawData struct corresponds to a similar C++ struct described in 
the recipe Implementing indirect rendering with Vulkan:
#include <data/shaders/gltf/common_material.sp>
struct DrawData {
 uint transformId;
 uint materialId;
};
2. The buffer declarations and push constants correspond to the C++ buffers created in VKMesh, 
which were described in the recipe Implementing indirect rendering with Vulkan:
layout(std430, buffer_reference)
 readonly buffer TransformBuffer {
 mat4 model[];
};
layout(std430, buffer_reference)
 readonly buffer DrawDataBuffer {
 DrawData dd[];
};
layout(std430, buffer_reference)
 readonly buffer MaterialBuffer {
 MetallicRoughnessDataGPU material[];
};
layout(push_constant) uniform PerFrameData {
 mat4 viewProj;


 TransformBuffer transforms;
 DrawDataBuffer drawData;
 MaterialBuffer materials;
 uint texSkyboxIrradiance;
} pc;
Now, let’s explore the vertex shader located in Chapter08/02_SceneGraph/src/main.vert.
1. The vertex shader inputs include vec3 for vertex position, vec2 for texture coordinates, and a 
normal vector. The outputs contain a world position and an integer material ID:
#include <Chapter08/02_SceneGraph/src/common.sp>
layout (location=0) in vec3 in_pos;
layout (location=1) in vec2 in_tc;
layout (location=2) in vec3 in_normal;
layout (location=0) out vec2 uv;
layout (location=1) out vec3 normal;
layout (location=2) out vec3 worldPos;
layout (location=3) out flat uint materialId;
2. As explained in the recipe Implementing indirect rendering with Vulkan, each indirect draw command stores the draw data ID (which is a mesh ID) in the firstInstance member field, which 
is passed into GLSL shaders as gl_BaseInstance. We now use this built-in variable to index into 
the DrawData buffer and retrieve the corresponding values for transformId and materialId:
void main() {
 mat4 model = pc.transforms.model[
 pc.drawData.dd[gl_BaseInstance].transformId];
 gl_Position = pc.viewProj * model * vec4(in_pos, 1.0);
 uv = vec2(in_tc.x, 1.0-in_tc.y);
 normal = transpose( inverse(mat3(model)) ) * in_normal;
 vec4 posClip = model * vec4(in_pos, 1.0);
 worldPos = posClip.xyz/posClip.w;
 materialId = pc.drawData.dd[gl_BaseInstance].materialId;
}
The fragment shader is more complex. First, let’s take a look at the helper function runAlphaTest(), 
which is defined in data/shaders/AlphaTest.sp. This function helps us handle transparency by 
discarding fragments that fall below a certain alpha threshold, ensuring that only visible parts of 
transparent objects are rendered.


void runAlphaTest(float alpha, float alphaThreshold) {
 if (alphaThreshold > 0.0) {
 mat4 thresholdMatrix = mat4(
 1.0 /17.0, 9.0/17.0, 3.0/17.0, 11.0/17.0,
 13.0/17.0, 5.0/17.0, 15.0/17.0, 7.0/17.0,
 4.0 /17.0, 12.0/17.0, 2.0/17.0, 10.0/17.0,
 16.0/17.0, 8.0/17.0, 14.0/17.0, 6.0/17.0 );
 alpha = clamp(alpha - 0.5 *
 thresholdMatrix[int(mod(gl_FragCoord.x, 4.0))]
 [int(mod(gl_FragCoord.y, 4.0))],
 0.0, 1.0);
 if (alpha < alphaThreshold) discard;
 }
}
Now let’s dive into the actual GLSL fragment shader, which handles material and lighting calculations. 
It can be found at Chapter08/02_SceneGraph/src/main.frag.
1. The shared GLSL header files data/shaders/common.sp and data/shaders/AlphaTest.sp, 
mentioned earlier, provide key declarations and functions for material handling and alpha 
transparency. The data/shaders/UtilsPBR.sp file contains the perturbNormal()function, 
which adjusts the normal vectors for bump mapping. This function was described in Chapter 6:
#include <Chapter08/02_SceneGraph/src/common.sp>
#include <data/shaders/AlphaTest.sp>
#include <data/shaders/UtilsPBR.sp>
layout (location=0) in vec2 uv;
layout (location=1) in vec3 normal;
layout (location=2) in vec3 worldPos;
layout (location=3) in flat uint materialId;
layout (location=0) out vec4 out_FragColor;
2. In this chapter, we use a simplified lighting model to keep the focus on the scene structure. 
However, we still use the same material struct MetallicRoughnessDataGPU, which ensures that 
we can reuse this code in later chapters with more complex lighting models:
Note
To simplify things and avoid sorting the scene, alpha transparency is simulated using a 
technique called dithering, combined with punch-through transparency. This approach 
avoids the complexity of traditional transparency handling. For more insights, you can 
refer to this article by Alex Charlton: http://alex-charlton.com/posts/Dithering_
on_the_GPU. Below, we provide the final implementation for your convenience:


void main() {
 MetallicRoughnessDataGPU mat = pc.materials.material[materialId];
 vec4 emissiveColor = 
 vec4(mat.emissiveFactorAlphaCutoff.rgb, 0) *
 textureBindless2D(mat.emissiveTexture, 0, uv);
 vec4 baseColor = mat.baseColorFactor *
 (mat.baseColorTexture > 0 ?
 textureBindless2D( mat.baseColorTexture, 0, uv) :
 vec4(1.0));
3. Here, we use runAlphaTest() to simulate transparency by combining alpha-test and punchthrough transparency. A naïve check against a constant alpha-cutoff value would cause alpha-tested foliage geometry to disappear at greater distances from the camera. To prevent this, 
we use a simple trick described in https://bgolus.medium.com/anti-aliased-alpha-testthe-esoteric-alpha-to-coverage-8b177335ae4f, where we scale the alpha-cutoff value using 
fwidth() to achieve better anti-aliasing for alpha-tested geometry:
 runAlphaTest(baseColor.a,
 mat.emissiveFactorAlphaCutoff.w / max(32.0 * fwidth(uv.x), 1.0));
4. This approach is a “poor man’s” version of normal mapping, providing a simplified implementation. Here, n is the world-space normal vector:
 vec3 n = normalize(normal);
 vec3 normalSample =
 textureBindless2D(mat.normalTexture, 0, uv).xyz;
 if (length(normalSample) > 0.5) {
 n = perturbNormal(n, worldPos, normalSample, uv);
 }
5. We hardcode two directional lights directly into the shader code for simplicity. We use an ad 
hoc scaling factor if there’s a skybox irradiance map available:
 float NdotL1 = clamp(
 dot(n, normalize(vec3(-1, 1, +0.5))), 0.1, 1.0);
 float NdotL2 = clamp(
 dot(n, normalize(vec3(+1, 1, -0.5))), 0.1, 1.0);
 const bool hasSkybox = pc.texSkyboxIrradiance > 0;
 float NdotL =
 (hasSkybox ? 0.2 : 1.0) * (NdotL1 + NdotL2);
If you want to learn a more accurate method for transforming normal vectors, 
be sure to check out this article Transforming Normals by Eric Lengyel: https://
terathon.com/blog/transforming-normals.html.


6. The IBL component for diffuse lighting here is kept simple and shiny, without aiming for any 
PBR accuracy. The primary goal of this chapter is to demonstrate scene rendering techniques, 
without adding the complexity of realistic material setups. We manually rotate the skybox to 
adjust the incoming IBL for a more aesthetically pleasing result. This same hack is applied to 
the skybox rendering fragment shader found in Chapter08/02_SceneGraph/src/skybox.frag:
 const vec4 f0 = vec4(0.04);
 vec3 sky = vec3(-n.x, n.y, -n.z); // rotate skybox
 vec4 diffuse = hasSkybox ?
 (textureBindlessCube(pc.texSkyboxIrradiance, 0, sky) +
 vec4(NdotL)) * baseColor * (vec4(1.0) - f0) :
 NdotL * baseColor;
 out_FragColor = emissiveColor + diffuse;
}
That’s it for the GLSL shaders. Before running the sample application, let’s revisit the scene merging 
functions mentioned earlier – mergeScenes(), mergeMeshData(), and mergeMaterialLists() – to 
explore how they work.

### How it works...

The mergeMeshData() routine takes a vector of MeshData instances and creates a new MeshFileHeader
instance while simultaneously copying all the indices and vertices into the output object MeshData m:
1. First, we merge the std::vector containers for the index buffers, vertex buffers, meshes, and 
bounding boxes. We retrieve the size of the vertex format for this mesh using the function Me
shData::streams::getVertexSize():
MeshFileHeader mergeMeshData(
 MeshData& m, const std::vector<MeshData*> md)
{
 uint32_t numTotalVertices = 0;
 uint32_t numTotalIndices = 0;
 if (!md.empty())m.streams = md[0]->streams;
 const uint32_t vertexSize = m.streams.getVertexSize();
 uint32_t offset = 0;
 uint32_t mtlOffset = 0;
 for (const MeshData* i : md) {
 mergeVectors(m.indexData, i->indexData);
 mergeVectors(m.vertexData, i->vertexData);
 mergeVectors(m.meshes, i->meshes);
 mergeVectors(m.boxes, i->boxes);


2. After merging the containers, we need to shift the index offset by the total size of the already 
merged indices and do the same for materials using the count of the already merged materials. The values of Mesh::vertexCount, Mesh::lodCount, and Mesh::vertexOffset remain 
unchanged because the vertex offsets are local and have already been baked into the indices:
 for (size_t j = 0; j != i->meshes.size(); j++) {
 m.meshes[offset + j].indexOffset += numTotalIndices;
 m.meshes[offset + j].materialID += mtlOffset;
 }
3. Next, we shift the individual indices by the total number of vertices that have already been 
merged:
 for (size_t j = 0;
 j != i->indexData.size(); j++) {
 m.indexData[numTotalIndices + j] += numTotalVertices;
 }
4. After processing each MeshData instance, we update the number of already merged meshes, 
materials, indices, and vertices. Since the vertex data size is calculated in bytes, we need to 
divide that number by vertexSize:
 offset += (uint32_t)i->meshes.size();
 mtlOffset += (uint32_t)i->materials.size();
 numTotalIndices += (uint32_t)i->indexData.size();
 numTotalVertices += (uint32_t)i->vertexData.size() / vertexSize;
 }
5. The resulting MeshFileHeader instance contains the total size of the index and vertex data 
arrays:
 return MeshFileHeader {
 .magicValue = 0x12345678,
 .meshCount = (uint32_t)offset,
 .indexDataSize = numTotalIndices * sizeof(uint32_t),
 .vertexDataSize = m.vertexData.size(),
 };
}
6. The mergeVectors() helper function is a templated one-liner that appends the second vector 
v2 to the end of the first vector v1:
template <typename T> void mergeVectors(
 vector<T>& v1, const vector<T>& v2) {
 v1.insert(v1.end(), v2.begin(), v2.end());
}


Along with merging mesh data, we need to aggregate the material descriptions from different meshes 
into a single collection.
1. The mergeMaterialLists() function creates one combined vector of texture filenames and a 
vector of material descriptions, ensuring that texture indices are correctly updated:
void mergeMaterialLists(
 const vector<vector<Material>*>& oldMaterials,
 const vector<vector<std::string>*>& oldTextures,
 vector<Material>& allMaterials,
 vector<std::string>& newTextures)
{
2. The merge process begins by creating a unified list of materials. Each material list index is 
linked with a texture, allowing us to later determine the list the texture belongs to:
 unordered_map<size_t, size_t> materialToTextureList;
 for (size_t midx = 0; midx != oldMaterials.size(); midx++) {
 for (const Material& m : *oldMaterials[midx]) {
 allMaterials.push_back(m);
 materialToTextureList[allMaterials.size()-1] = midx;
 }
 }
3. The combined texture container newTextureNames holds only unique texture filenames. The 
indices of these texture files are stored in a map, which is then used to update the texture 
references in the material descriptors:
 unordered_map<std::string, int> newTextureNames;
 for (const vector<std::string>* tl : oldTextures)
 {
 for (const std::string& file : *tl) {
 newTextureNames[file] = addUnique(newTextures, file);
 }
 }
4. The replaceTexture() lambda function takes a texture index from an old texture container 
and assigns it a texture index from the newTextureNames array:
 auto replaceTexture = [&materialToTextureList,
 &oldTextures, &newTextureNames](
 int mtlId, int* textureID) {
 if (*textureID == -1)return;
 const size_t listIdx = materialToTextureList[mtlId];
 const std::vector<std::string>& texList = *oldTextures[listIdx];


 const std::string& texFile = texList[*textureID];
 *textureID = newTextureNames[texFile];
 };
5. The final loop iterates over all materials and adjusts the texture indices accordingly:
 for (size_t i = 0; i < allMaterials.size(); i++) {
 Material& m = allMaterials[i];
 replaceTexture(i, &m.baseColorTexture);
 replaceTexture(i, &m.emissiveTexture);
 replaceTexture(i, &m.normalTexture);
 replaceTexture(i, &m.opacityTexture);
 }
}
To merge multiple object collections, we need one more routine that combines multiple scene hierarchies into a single large scene graph. The scene data is defined by the Hierarchy item array, local and 
global transforms, and associative arrays for meshes, materials, and scene nodes. Similar to merging 
mesh index and vertex data, this merge routine essentially involves merging individual arrays and 
then adjusting the indices within each scene node.
1. The shiftNodes() routine increments individual fields of the scene hierarchy structure by 
the given shiftAmount value:
void shiftNodes(
 Scene& scene, int startOffset, int nodeCount,
 int shiftAmount)
{
 auto shiftNode = [shiftAmount](Hierarchy& node) {
 if (node.parent > -1) node.parent +=shiftAmount;
 if (node.firstChild > -1 ) node.firstChild += shiftAmount;
 if (node.nextSibling > -1) node.nextSibling += shiftAmount;
 if (node.lastSibling > -1) node.lastSibling += shiftAmount;
 };
 for (int i = 0; i < nodeCount; i++)
 shiftNode(scene.hierarchy[i + startOffset]);
}
2. The mergeMaps() helper routine adds the contents of unordered map otherMap to the output 
map m, while shifting its integer values by the specified itemOffset amount:
using ItemMap = std::unordered_map<uint32_t, uint32_t>;
void mergeMaps(ItemMap& m, const ItemMap& otherMap,
 int indexOffset, int itemOffset)
{


 for (const auto& i : otherMap)
 m[i.first + indexOffset] = i.second + itemOffset;
}
Now that we have all the utility functions in place, we can merge two Scene objects. The mergeScenes()
routine creates a new root scene node called "NewRoot" and attaches all the root scene nodes from 
the scenes being merged as child nodes of the "NewRoot" node. Let’s examine the implementation in 
shared/Scene/Scene.cpp:
1. This source code bundle with this routine has two extra parameters, mergeMeshes and 
mergeMaterials, which allow the creation of composite scenes with shared mesh and material data. We omit these inessential parameters to shorten the description:
void mergeScenes(
 Scene& scene, const std::vector<Scene*>& scenes,
 const std::vector<glm::mat4>& rootTransforms,
 const std::vector<uint32_t>& meshCounts,
 bool mergeMeshes, bool mergeMaterials)
{
 scene.hierarchy = { {
 .parent = -1,
 .firstChild = 1,
 .nextSibling = -1,
 .lastSibling = -1,
 .level = 0 } };
2. The array of names, along with the local and global transforms, initially contains a single 
element, "NewRoot":
 scene.nameForNode[0] = 0;
 scene.nodeNames = { "NewRoot" };
 scene.localTransform.push_back(glm::mat4(1.f));
 scene.globalTransform.push_back(glm::mat4(1.f));
 if (scenes.empty()) return;
3. While iterating through the scenes, we merge and shift all the arrays and maps. The following 
variables keep track of the item counts in the output scene:
 int offs = 1;
 int meshOffs = 0;
 int nameOffs = (int)scene.nodeNames.size();
 int materialOfs = 0;
 auto meshCount = meshCounts.begin();
 if (!mergeMaterials)
 scene.materialNames = scenes[0]->materialNames;


4. This implementation isn’t the most efficient one, mainly because it combines the merging of 
all scene graph components into a single routine. However, it’s simple enough:
 for (const Scene* s : scenes) {
 mergeVectors(scene.localTransform, s->localTransform);
 mergeVectors(scene.globalTransform, s->globalTransform);
 mergeVectors(scene.hierarchy, s->hierarchy);
 mergeVectors(scene.nodeNames, s->nodeNames);
 if (mergeMaterials)
 mergeVectors(scene.materialNames, s->materialNames);
 const int nodeCount = (int)s->hierarchy.size();
 shiftNodes(scene, offs, nodeCount, offs);
 mergeMaps(scene.meshForNode,
 s->meshForNode, offs, mergeMeshes ? meshOffs : 0);
 mergeMaps(scene.materialForNode,
 s->materialForNode, offs, mergeMaterials ? materialOfs : 0);
 mergeMaps(scene.nameForNode, s->nameForNode, offs, nameOffs);
5. During each iteration, we add the sizes of the current arrays to the global offsets:
 offs += nodeCount;
 materialOfs += (int)s->materialNames.size();
 nameOffs += (int)s->nodeNames.size();
 if (mergeMeshes) {
 meshOffs += *meshCount;
 meshCount++;
 }
 }
6. Logically, the routine is complete, but there is one more step. Each scene node has a cached 
index of its last sibling, which we need to update for the new root nodes. We also assign a new 
local transform to each root node in the following loop:
 offs = 1;
 int idx = 0;
 for (const Scene* s : scenes) {
 const int nodeCount = (int)s->hierarchy.size();
 const bool isLast = (idx == scenes.size()-1);
7. Calculate the new “next sibling” for the old scene roots and attach them to the new root:
 const int next = isLast ? -1 : offs + nodeCount;
 scene.hierarchy[offs].nextSibling = next;
 scene.hierarchy[offs].parent = 0;


8. Transform the old root nodes, if the transforms are provided:
 if (!rootTransforms.empty())
 scene.localTransform[offs] =
 rootTransforms[idx] * scene.localTransform[offs];
 offs += nodeCount;
 idx++;
 }
9. At the end of the routine, increment all the depth-from-root levels of the scene nodes, but leave 
the "NewRoot" node unchanged, hence the +1:
 for (auto i = scene.hierarchy.begin() + 1;
 i != scene.hierarchy.end(); i++)
 i->level++;
}
Now that our arsenal of scene graph management routines is complete, we’ve reached the end of what 
is likely to be the most complex chapter in this book, with the highest cognitive load and a focus on 
data structures. The upcoming chapters will be more graphics-oriented, we promise.
By the way, before we move on to the next chapter, let’s revisit our sample application in Chapter08/03_
LargeScene and run it. You should see the Lumberyard Bistro scene, as shown in the following screenshot.
Note
As a keen reader might have noticed while going through this chapter, we focused more 
on runtime loading and rendering performance rather than preprocessing performance. 
That’s true. The reason is simple: using STL containers and algorithms often leads to 
shorter but slower code. Making every scene manipulation faster would add complexity 
and make that already quite complex code even harder to understand.


Figure 8.9: The Lumberyard Bistro scene with textures
Fly around the scene and explore the transparent windows and the interior of the bistro. You can 
use the scene tree UI on the left side to select individual meshes, and they will be highlighted with a 
green bounding box.
Now, let’s move on to the next chapter and explore glTF animations to bring some dynamics to our 
static scenes.
There’s more...
One might wonder why we went through all these pages of data structures. Was the cost of fast scene 
loading really worth it? We believe it was. As proof, try running samples from other books that use 
the same Bistro 3D model – you’ll likely be disappointed.
The real benefit of this approach is that the scene loading is now two or even three orders of magnitude faster compared to a naïve approach. This makes it possible to run Debug builds on much larger 
datasets than just the Bistro scene. Imagine loading hundreds of different meshes the size of Bistro 
and still being able to run your rendering engine in Debug mode. This is one of those things that sets 
hobby projects apart from professional 3D engines.


Unlock this book’s exclusive 
benefits now
This book comes with additional benefits 
designed to elevate your learning experience. 
Note: Have your purchase invoice ready before you begin. https://www.packtpub.com/unlock/9781803248110
