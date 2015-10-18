/*
	Added by Lifan Wu
	Oct 17, 2015
*/

#pragma once
#if !defined(__MISTUBA_RENDER_LIGHT_TREE_H_)
#define __MITSUBA_RENDER_LIGHT_TREE_H_	

#include <mitsuba/render/vpl.h>
#include <mitsuba/core/cone.h>

MTS_NAMESPACE_BEGIN

enum ELightNodeType {
	EPointLightNode = 0,
	EDirectionalLightNode,
	ESurfaceLightNode
};

struct LightNode {
	LightNode() {}
	virtual ~LightNode() {}
	Spectrum P;
	//virtual bool IsLeaf() const = 0;
	//virtual ELightNodeType GetNodeType() const = 0;
};

MTS_NAMESPACE_END

#endif