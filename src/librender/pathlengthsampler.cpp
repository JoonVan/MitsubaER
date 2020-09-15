#include <mitsuba/render/pathlengthsampler.h>
#include <boost/algorithm/string.hpp>

MTS_NAMESPACE_BEGIN

PathLengthSampler::PathLengthSampler(const Properties &props)
	: ConfigurableObject(props){
	std::string modulationType = boost::to_lower_copy(
					props.getString("modulation", "none"));

	m_decompositionMinBound = props.getFloat("minBound", 0.0f);
	m_decompositionMaxBound = props.getFloat("maxBound", 0.0f);
	m_lambda 				= props.getFloat("lambda",1);
	m_phase 				= props.getFloat("phase",0)*M_PI/180;
	m_P						= props.getInteger("P",32);
	m_neighbors				= props.getInteger("neighbors",3);

	if (modulationType == "none") {
		m_modulationType = ENone;
	} else if (modulationType == "sine") {
		m_modulationType = ESine;
	} else if (modulationType == "square") {
		m_modulationType = ESquare;
	} else if (modulationType == "hamiltonian") {
		m_modulationType = EHamiltonian;
	} else if (modulationType == "mseq") {
		m_modulationType = EMSeq;
	} else if (modulationType == "depthselective") {
		m_modulationType = EDepthSelective;
	} else {
		SLog(EError, "The \"modulation\" parameter must be equal to"
			"either \"none\", \"square\", or \"hamiltonian\", or \"mseq\", or \"depthselective\"!");
	}

	m_areaUnderCorrelationGraph = areaUnderCorrelationGraph(1e6); // use a million point numerical approximation
}
PathLengthSampler::PathLengthSampler(Stream *stream, InstanceManager *manager)
	: ConfigurableObject(stream, manager){
	m_decompositionMinBound = stream->readFloat();
	m_decompositionMaxBound = stream->readFloat();
	m_modulationType		= (EModulationType)stream->readUInt();
	m_lambda				= stream->readFloat();
	m_phase					= stream->readFloat();
	m_P						= stream->readUInt();
	m_neighbors				= stream->readUInt();
}

void PathLengthSampler::addChild(const std::string &name, ConfigurableObject *child) {  }

void PathLengthSampler::serialize(Stream *stream, InstanceManager *manager) const {
	ConfigurableObject::serialize(stream, manager);

	stream->writeFloat(m_decompositionMinBound);
	stream->writeFloat(m_decompositionMaxBound);
	stream->writeUInt(m_modulationType);
	stream->writeFloat(m_lambda);
	stream->writeFloat(m_phase);
	stream->writeUInt(m_P);
	stream->writeUInt(m_neighbors);
}

void PathLengthSampler::configure() {  }

PathLengthSampler::~PathLengthSampler() { }

Float PathLengthSampler::correlationFunction(const Float& t) const {
	Float pathLength = t;
	switch(m_modulationType){
		case ENone:{
			SLog(EError, "Cannot call correlation function when the modulation type is not defined");
			break;
		}
		case ESine:{
			pathLength = pathLength + m_phase*m_lambda*INV_PI/2;
			return cos(pathLength*2*M_PI/m_lambda);
			break;
		}
		case ESquare:{
			pathLength = pathLength + m_phase*m_lambda*INV_PI/2;
			return 4/m_lambda*(fabs(fmod(pathLength, m_lambda)-m_lambda/2) - m_lambda/4);
			break;
		}
		case EHamiltonian:{
			pathLength = pathLength + m_phase*m_lambda*INV_PI/2;
			pathLength = fmod(pathLength, m_lambda);
			if(pathLength < m_lambda/6){
				return 6*pathLength/m_lambda;
			}else if(pathLength < m_lambda/2 	&& pathLength >= m_lambda/6){
				return 1.0;
			}else if(pathLength < 2*m_lambda/3 	&& pathLength >= m_lambda/2){
				return 1 - (pathLength - m_lambda/2)*6/m_lambda;
			}else{
				return 0;
			}
			break;
		}
		case EMSeq:{
			return mSeq(pathLength, m_phase);
			break;
		}
		case EDepthSelective:{
			Float value = 0;
			for(int i = 0; i < m_neighbors; i++){
				value += mSeq(pathLength, m_phase - i*(2*M_PI)/m_P);
			}
			value -= (float)(m_neighbors-1)/m_P;
			return value;
			break;
		}
		default:
			SLog(EError, "Modulation type is not defined");
	}
	return 0;
}

Float PathLengthSampler::areaUnderCorrelationGraph(const int& n) const{
	return areaUnderRestrictedCorrelationGraph(m_decompositionMinBound, m_decompositionMaxBound, n);
}

Float PathLengthSampler::samplePathLengthTarget(ref<Sampler> sampler) const{
	return sampleRestrictedPathLengthTarget(m_decompositionMinBound, m_decompositionMaxBound, sampler);
}


Float PathLengthSampler::areaUnderRestrictedCorrelationGraph(const Float& plMin, const Float& plMax, const int& n) const{

	switch(m_modulationType){
		case ENone:{
			return plMax - plMin;
			break;
		}
		case ESine:{
			Float thetaMin 	= plMin * 2 * M_PI/m_lambda + m_phase;
			Float AreaMin  	= floor(thetaMin * INV_TWOPI) * 4;
			thetaMin -= floor(thetaMin * INV_TWOPI) * 2 * M_PI;
			if(thetaMin < M_PI/2){
				AreaMin += sin(thetaMin);
			}else if(thetaMin < 3*M_PI/2){
				AreaMin += 2 - sin(thetaMin);
			}else{
				AreaMin += 4 + sin(thetaMin);
			}

			Float thetaMax 	= plMax * 2 * M_PI/m_lambda + m_phase;
			Float AreaMax  	= floor(thetaMax * INV_TWOPI) * 4;
			thetaMax -= floor(thetaMax * INV_TWOPI) * 2 * M_PI;
			if(thetaMax < M_PI/2){
				AreaMax += sin(thetaMax);
			}else if(thetaMax < 3*M_PI/2){
				AreaMax += 2 - sin(thetaMax);
			}else{
				AreaMax += 4 + sin(thetaMax);
			}

			return (m_lambda * INV_TWOPI * (AreaMax - AreaMin));
			break;
		}
		case ESquare:{
			Float plMinTemp = plMin + m_phase * m_lambda * INV_TWOPI;
			Float AreaMin  	= floor(2 * plMinTemp/m_lambda) * m_lambda/4;
			plMinTemp -= floor(2 * plMinTemp/m_lambda) * m_lambda/2;
			if(plMinTemp < m_lambda/4){
				AreaMin += plMinTemp * (1 - 2/m_lambda * plMinTemp);
			}else{
				AreaMin += m_lambda/4 - plMinTemp * (1 - 2/m_lambda * plMinTemp);
			}

			Float plMaxTemp = plMax + m_phase * m_lambda * INV_TWOPI;
			Float AreaMax  	= floor(2 * plMaxTemp/m_lambda) * m_lambda/4;
			plMaxTemp -= floor(2 * plMaxTemp/m_lambda) * m_lambda/2;
			if(plMaxTemp < m_lambda/4){
				AreaMax += plMaxTemp * (1 - 2/m_lambda * plMaxTemp);
			}else{
				AreaMax += m_lambda/4 - plMaxTemp * (1 - 2/m_lambda * plMaxTemp);
			}

			return (AreaMax - AreaMin);
			break;
		}
		case EDepthSelective:{
			Float inv_P = 1/(Float)m_P;

			Float plMinTemp = plMin + m_phase * m_lambda * INV_TWOPI;
			Float AreaMin  	= floor(plMinTemp/m_lambda) * (m_lambda*inv_P*(m_neighbors + 1 - m_neighbors*inv_P));
			plMinTemp -= floor(plMinTemp/m_lambda) * m_lambda;

			if(plMinTemp < (m_neighbors-1)*m_lambda*inv_P){
				AreaMin += plMinTemp;
			}else if(plMinTemp >= (m_neighbors-1)*m_lambda*inv_P && plMinTemp < m_neighbors*m_lambda*inv_P){
				AreaMin += (m_neighbors-1)*m_lambda*inv_P + (1 + (m_neighbors-1)*(1 - inv_P))*(plMinTemp - (m_neighbors-1)*m_lambda*inv_P) - (m_P-1)/(2*m_lambda)*(plMinTemp*plMinTemp - (pow((m_neighbors-1)*m_lambda*inv_P,2)));
			}else if(plMinTemp >= (m_neighbors)*m_lambda*inv_P && plMinTemp < (1 - inv_P)*m_lambda){
				AreaMin += (m_neighbors-1)*m_lambda*inv_P + m_lambda*inv_P/2*(1 - inv_P) + inv_P*(plMinTemp - (m_neighbors-1)*m_lambda*inv_P);
			}else{
				AreaMin += (m_neighbors-1)*m_lambda*inv_P + m_lambda*inv_P/2*(1 - inv_P) + inv_P*((1 - inv_P)*m_lambda - (m_neighbors-1)*m_lambda*inv_P) + (2-m_P)*(plMinTemp- ( 1 - inv_P)*m_lambda) + 1/(2*m_lambda) * (plMinTemp*plMinTemp - (1 - inv_P)*(1 - inv_P)*m_lambda*m_lambda)*(m_P-1);
			}

			Float plMaxTemp = plMax + m_phase * m_lambda * INV_TWOPI;
			Float AreaMax  	= floor(plMaxTemp/m_lambda) * (m_lambda*inv_P*(m_neighbors + 1 - m_neighbors*inv_P));
			plMaxTemp -= floor(plMaxTemp/m_lambda) * m_lambda;

			if(plMaxTemp < (m_neighbors-1)*m_lambda*inv_P){
				AreaMax += plMaxTemp;
			}else if(plMaxTemp >= (m_neighbors-1)*m_lambda*inv_P && plMaxTemp < m_neighbors*m_lambda*inv_P){
				AreaMax += (m_neighbors-1)*m_lambda*inv_P + (1 + (m_neighbors-1)*(1 - inv_P))*(plMaxTemp - (m_neighbors-1)*m_lambda*inv_P) - (m_P-1)/(2*m_lambda)*(plMaxTemp*plMaxTemp - (pow((m_neighbors-1)*m_lambda*inv_P,2)));
			}else if(plMaxTemp >= (m_neighbors)*m_lambda*inv_P && plMaxTemp < (1 - inv_P)*m_lambda){
				AreaMax += (m_neighbors-1)*m_lambda*inv_P + m_lambda*inv_P/2*(1 - inv_P) + inv_P*(plMaxTemp - (m_neighbors-1)*m_lambda*inv_P);
			}else{
				AreaMax += (m_neighbors-1)*m_lambda*inv_P + m_lambda*inv_P/2*(1 - inv_P) + inv_P*((1 - inv_P)*m_lambda - (m_neighbors-1)*m_lambda*inv_P) + (2-m_P)*(plMaxTemp- ( 1 - inv_P)*m_lambda) + 1/(2*m_lambda) * (plMaxTemp*plMaxTemp - (1 - inv_P)*(1 - inv_P)*m_lambda*m_lambda)*(m_P-1);
			}

			return (AreaMax - AreaMin);

			break;
		}
		default:
			Float h = (plMax-plMin)/(n-1);
			Float value = 0.5*( fabs(correlationFunction(plMax))+ fabs(correlationFunction(plMin)));
			for(int i=2; i < n; i++){
				value += fabs(correlationFunction( plMin + h*(i-1) ));
			}
			value *= h;
			return value;
	}
	return 0;
}

Float PathLengthSampler::sampleRestrictedPathLengthTarget(const Float& plMin, const Float& plMax, ref<Sampler> sampler) const{

	int rejects = 0;

	switch(m_modulationType){
		case ENone:{
			return plMin+(plMax-plMin)*sampler->nextFloat();
			break;
		}
		default:
			while(true){
				Float t = plMin+(plMax-plMin)*sampler->nextFloat();
				Float r = sampler->nextFloat();
				if(r < fabs(correlationFunction(t))){
					return t;
				}
				rejects++;
				if(rejects > 1e6){
					SLog(EError, "Rejects exceed 1e6.");
				}
			}
	}
	return 0;
}

MTS_IMPLEMENT_CLASS(PathLengthSampler, true, ConfigurableObject)

MTS_NAMESPACE_END
