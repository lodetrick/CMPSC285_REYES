#[cfg(test)]
mod tests {
	use crate::ri::*;

	#[test]
	fn transform_correct() {
		{
			RiTransformBegin();
			RiTranslate(1.,2.,3.);
			RiScale(2.,3.,4.);
			assert_eq!(RiApplyTransform(1., 1., 1.), RtVector::new(3., 5., 7.));
			RiTransformEnd();
		}

		{
			RiTransformBegin();
			RiTranslate(8., 0., 0.);
			RiRotate(20., 1., 0., 0.);
			assert_eq!(RiApplyTransform(0., 0., 0.), RtVector::new(8.,0.,0.));
			RiTransformEnd();
		}
	}
}