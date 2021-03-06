#pragma once

class MpmSimulator: public MPxLocatorNode
{
public:
	enum InitType
	{
		INIT_FROM_VDB = 0,
		INIT_SPHERE	  = 1,
		INIT_TWO_SPHERES=2
	};

	MpmSimulator(void);
	~MpmSimulator(void);
	void				postConstructor();

	virtual void		draw( M3dView & view, const MDagPath & path, M3dView::DisplayStyle style, M3dView:: DisplayStatus );
	virtual bool		isBounded() const{return true;}
	virtual MBoundingBox boundingBox() const;

	virtual MStatus		compute( const MPlug& plug, MDataBlock& data );
	static  void*		creator();
	static  MStatus		initialize();

	static MTypeId		m_id;
	static const char*  m_nodeName;

	bool				initSolver();
	bool				stepSolver();
	int					getNumSampledParticle();

private:
	void				drawIcon();
	void				drawCell();
	int					getCurFrame();
	bool				initParticle();
	MBoundingBox		m_box;
	MpmCore				m_core;

	static MObject		s_boxMin;			// 
	static MObject		s_boxMax;
	static MObject		s_cellSize;
	static MObject		s_nParticlePerCell;
	static MObject		s_boundary;

	// physical param
	static MObject		s_particleDensity;
	static MObject		s_youngs;
	static MObject		s_poisson;
	static MObject		s_hardening;
	static MObject		s_criticalCompression;
	static MObject		s_criticalStretch;
	static MObject		s_friction;
	static MObject		s_flip;
	static MObject		s_gravity;
	static MObject		s_deltaT;
	static MObject		s_nSubStep;

	// input particle
	static MObject		s_initParticle;
	static MObject		s_initType;
	static MObject		s_initTrans0;
	static MObject		s_initTrans1;
	static MObject		s_initDeltaTime;

	// particle
	static MObject		s_particleStr;
	static MObject		s_inputParticle;
	static MObject		s_outputParticle;
	static MObject		s_sampleRate;
	static MObject		s_numSampledParticle;

	static const char*  s_boxMinName[2];
	static const char*	s_boxMaxName[2];
	static const char*  s_cellSizeName[2];
	static const char*	s_nParticlePerCellName[2];
	static const char*  s_boundaryName[2];

	static const char*  s_particleDensityName[2];
	static const char*	s_youngsName[2];
	static const char*	s_poissonName[2];
	static const char*	s_hardeningName[2];
	static const char*	s_criticalCompressionName[2];
	static const char*	s_criticalStretchName[2];
	static const char*	s_frictionName[2];
	static const char*	s_flipName[2];
	static const char*	s_gravityName[2];
	static const char*	s_deltaTName[2];
	static const char*  s_nSubStepName[2];

	static const char*	s_initParticleName[2];
	static const char*	s_initTypeName[2];
	static const char*	s_initTrans0Name[2];
	static const char*  s_initTrans1Name[2];
	static const char*  s_initDeltaTimeName[2];

	static const char*  s_particleStrName[2];
	static const char*  s_inputParticleName[2];
	static const char*  s_outputParticleName[2];
	static const char*  s_sampleRateName[2];
	static const char*  s_numSampledParticleName[2];
};

