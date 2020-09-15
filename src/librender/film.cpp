/*
    This file is part of Mitsuba, a physically based rendering system.

    Copyright (c) 2007-2014 by Wenzel Jakob and others.

    Mitsuba is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License Version 3
    as published by the Free Software Foundation.

    Mitsuba is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <mitsuba/render/film.h>
#include <mitsuba/core/plugin.h>
#include <boost/algorithm/string.hpp>

MTS_NAMESPACE_BEGIN

Film::Film(const Properties &props)
 : ConfigurableObject(props) {
	bool isMFilm = boost::to_lower_copy(props.getPluginName()) == "mfilm";

	/* Horizontal and vertical film resolution in pixels */
	m_size = Vector2i(
		props.getInteger("width", isMFilm ? 1 : 768),
		props.getInteger("height", isMFilm ? 1 : 576)
	);
	/* Crop window specified in pixels - by default, this
	   matches the full sensor area */
	m_cropOffset = Point2i(
		props.getInteger("cropOffsetX", 0),
		props.getInteger("cropOffsetY", 0)
	);
	m_cropSize = Vector2i(
		props.getInteger("cropWidth", m_size.x),
		props.getInteger("cropHeight", m_size.y)
	);
	if (m_cropOffset.x < 0 || m_cropOffset.y < 0 ||
		m_cropSize.x <= 0 || m_cropSize.y <= 0 ||
		m_cropOffset.x + m_cropSize.x > m_size.x ||
		m_cropOffset.y + m_cropSize.y > m_size.y )
		Log(EError, "Invalid crop window specification!");

	/* If set to true, regions slightly outside of the film
	   plane will also be sampled, which improves the image
	   quality at the edges especially with large reconstruction
	   filters. */
	m_highQualityEdges = props.getBoolean("highQualityEdges", false);

	std::string decompositionType = boost::to_lower_copy(
					props.getString("decomposition", "none"));

	if (decompositionType == "none") {
		m_decompositionType = ESteadyState;
	} else if (decompositionType == "transient") {
		m_decompositionType = ETransient;
	} else if (decompositionType == "bounce") {
		m_decompositionType = EBounce;
	} else {
		Log(EError, "The \"decomposition\" parameter must be equal to"
			"either \"none\", \"transient\", or \"bounce\"!");
	}

	m_decompositionMinBound = props.getFloat("minBound", 0.0f);
	m_decompositionMaxBound = props.getFloat("maxBound", 0.0f);
	m_decompositionBinWidth = props.getFloat("binWidth", 1.0f);
	m_frames = ceil((m_decompositionMaxBound-m_decompositionMinBound)/m_decompositionBinWidth);

	m_pathLengthSampler = new PathLengthSampler(props);
	if( m_decompositionType == ESteadyState || (m_decompositionType == ETransient && m_pathLengthSampler->getModulationType()!= PathLengthSampler::ENone)){
		m_frames = 1;
	}

	m_calibratedTransient = props.getBoolean("calibratedTransient", false);

	m_forceBounces 	= props.getBoolean("forceBounce", false);
	m_sBounces  	= props.getInteger("sBounce", 0);
	m_tBounces 		= props.getInteger("tBounce", 0);

}

Film::Film(Stream *stream, InstanceManager *manager)
 : ConfigurableObject(stream, manager) {
	m_size = Vector2i(stream);
	m_cropOffset = Point2i(stream);
	m_cropSize = Vector2i(stream);
	m_highQualityEdges = stream->readBool();
	m_decompositionType = (EDecompositionType) stream->readUInt();
	m_decompositionMinBound = stream->readFloat();
	m_decompositionMaxBound = stream->readFloat();
	m_decompositionBinWidth = stream->readFloat();
	m_frames = stream->readSize();
	m_forceBounces = stream->readBool();
	m_sBounces = stream->readUInt();
	m_tBounces = stream->readUInt();
	m_filter = static_cast<ReconstructionFilter *>(manager->getInstance(stream));
	m_pathLengthSampler = static_cast<PathLengthSampler *>(manager->getInstance(stream));
}

Film::~Film() { }

void Film::serialize(Stream *stream, InstanceManager *manager) const {
	ConfigurableObject::serialize(stream, manager);
	m_size.serialize(stream);
	m_cropOffset.serialize(stream);
	m_cropSize.serialize(stream);
	stream->writeBool(m_highQualityEdges);
	stream->writeUInt(m_decompositionType);
	stream->writeFloat(m_decompositionMinBound);
	stream->writeFloat(m_decompositionMaxBound);
	stream->writeFloat(m_decompositionBinWidth);
	stream->writeSize(m_frames);
	stream->writeBool(m_forceBounces);
	stream->writeUInt(m_sBounces);
	stream->writeUInt(m_tBounces);
	manager->serialize(stream, m_filter.get());
	manager->serialize(stream, m_pathLengthSampler.get());
}

void Film::addChild(const std::string &name, ConfigurableObject *child) {
	const Class *cClass = child->getClass();

	if (cClass->derivesFrom(MTS_CLASS(ReconstructionFilter))) {
		Assert(m_filter == NULL);
		m_filter = static_cast<ReconstructionFilter *>(child);
	} else if (cClass->derivesFrom(MTS_CLASS(PathLengthSampler))) {
		Assert(m_pathLengthSampler == NULL);
		m_pathLengthSampler = static_cast<PathLengthSampler *>(child);
	}else {
		Log(EError, "Film: Invalid child node! (\"%s\")",
			cClass->getName().c_str());
	}
}

void Film::configure() {
	if (m_filter == NULL) {
		/* No reconstruction filter has been selected. Load a Gaussian filter by default */
		m_filter = static_cast<ReconstructionFilter *> (PluginManager::getInstance()->
				createObject(MTS_CLASS(ReconstructionFilter), Properties("gaussian")));
		m_filter->configure();
	}
}

MTS_IMPLEMENT_CLASS(Film, true, ConfigurableObject)
MTS_NAMESPACE_END
