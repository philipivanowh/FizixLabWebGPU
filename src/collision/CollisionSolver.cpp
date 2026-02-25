#include "collision/CollisionSolver.hpp"

#include "collision/Collisions.hpp"
#include "collision/ContactManifold.hpp"
#include "common/settings.hpp"
#include "math/Math.hpp"
#include "physics/Rigidbody.hpp"
#include "shape/Incline.hpp"

#include <algorithm>
#include <cmath>

// Velocity (pixels/s) below which a body is treated as "at rest" for
// the analytical static-friction path.  Tune to taste; 5 px/s works
// well at typical PIXELS_PER_METER values.
static constexpr float kRestVelThreshold = 5.0f;

// ─────────────────────────────────────────────────────────────────────────────
bool CollisionSolver::ResolveIfColliding(physics::Rigidbody &bodyA,
                                         physics::Rigidbody &bodyB)
{
    const collision::HitInfo hit = collision::Collide(bodyA, bodyB);
    if (!hit.result)
        return false;

    SeparateBodies(bodyA, bodyB, hit.normal * hit.depth);

    auto [contact1, contact2, contactCount] =
        collision::FindContactPoints(bodyA, bodyB);

    collision::ContactManifold contactManifold(
        bodyA, bodyB,
        hit.normal, hit.depth,
        contact1, contact2, contactCount);

    ResolveWithRotationAndFriction(contactManifold);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
void CollisionSolver::SeparateBodies(physics::Rigidbody &bodyA,
                                     physics::Rigidbody &bodyB,
                                     const math::Vec2 &mtv)
{
    const float maxCorrectionDepth = PhysicsConstants::MAX_PENETRATION_CORRECTION;
    const float mtvLength = mtv.Length();
    math::Vec2 correction = mtv;

    if (mtvLength > maxCorrectionDepth)
        correction = mtv * (maxCorrectionDepth / mtvLength);

    if (bodyA.bodyType == physics::RigidbodyType::Static)
        bodyB.Translate(correction);
    else if (bodyB.bodyType == physics::RigidbodyType::Static)
        bodyA.Translate(correction.Negate());
    else
    {
        bodyA.Translate(correction.Negate() / 2.0f);
        bodyB.Translate(correction / 2.0f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void CollisionSolver::ResolveWithRotationAndFriction(
    const collision::ContactManifold &contact)
{
    physics::Rigidbody &bodyA = *const_cast<physics::Rigidbody *>(contact.bodyA);
    physics::Rigidbody &bodyB = *const_cast<physics::Rigidbody *>(contact.bodyB);

    const math::Vec2 &normal = contact.normal;
    const int contactCount = contact.contactCount;

    const float restitution = std::min(bodyA.restitution, bodyB.restitution);

    float staticFriction;
    float kineticFriction;

    auto *inclineA = dynamic_cast<shape::Incline *>(&bodyA);
    auto *inclineB = dynamic_cast<shape::Incline *>(&bodyB);

    if (inclineA && inclineB)
    {
        // Both are inclines (rare) - use the one with higher friction
        staticFriction = std::max(inclineA->GetStaticFriction(), inclineB->GetStaticFriction());
        kineticFriction = std::max(inclineA->GetKineticFriction(), inclineB->GetKineticFriction());
    }
    else if (inclineA)
    {
        // Body A is an incline - its surface properties dominate
        staticFriction = inclineA->GetStaticFriction();
        kineticFriction = inclineA->GetKineticFriction();
    }
    else if (inclineB)
    {
        // Body B is an incline - its surface properties dominate
        staticFriction = inclineB->GetStaticFriction();
        kineticFriction = inclineB->GetKineticFriction();
    }
    else
    {
        // Neither is an incline - average the friction coefficients
        // (geometric mean is more physically accurate than arithmetic mean)
        staticFriction = std::sqrt(bodyA.staticFriction * bodyB.staticFriction);
        kineticFriction = std::sqrt(bodyA.kineticFriction * bodyB.kineticFriction);
    }
    contactList[0] = contact.contact1;
    contactList[1] = contact.contact2;

    // Zero all working arrays for this contact manifold
    for (int i = 0; i < contactCount; i++)
    {
        impulseList[i] = math::Vec2();
        raList[i] = math::Vec2();
        rbList[i] = math::Vec2();
        impulseFrictionList[i] = math::Vec2();
        jList[i] = 0.0f;
    }

    // =========================================================================
    // PASS 1 — Normal (restitution) impulses
    // =========================================================================
    for (int i = 0; i < contactCount; i++)
    {
        const math::Vec2 ra = contactList[i] - bodyA.pos;
        const math::Vec2 rb = contactList[i] - bodyB.pos;
        raList[i] = ra;
        rbList[i] = rb;

        const math::Vec2 raPerp(-ra.y, ra.x);
        const math::Vec2 rbPerp(-rb.y, rb.x);

        // Relative velocity at the contact point (includes angular contribution)
        const math::Vec2 relativeVelocity =
            (bodyB.linearVel + rbPerp * bodyB.angularVel) -
            (bodyA.linearVel + raPerp * bodyA.angularVel);

        const float contactVelMag = math::Vec2::Dot(relativeVelocity, normal);

        // Bodies already separating — no impulse needed for this contact
        if (contactVelMag > 0.0f)
            continue;

        const float raPerpDotN = math::Vec2::Dot(raPerp, normal);
        const float rbPerpDotN = math::Vec2::Dot(rbPerp, normal);
        const float denom = bodyA.invMass + bodyB.invMass + raPerpDotN * raPerpDotN * bodyA.invInertia + rbPerpDotN * rbPerpDotN * bodyB.invInertia;

        float j = -(1.0f + restitution) * contactVelMag;
        j /= denom;
        j /= static_cast<float>(contactCount);

        jList[i] = j;
        impulseList[i] = normal * j;
    }

    // Apply normal impulses + accumulate for FBD display
    // NOTE: AccumulateNormalImpulse must use += (not =) in Rigidbody.cpp
    for (int i = 0; i < contactCount; i++)
    {
        const math::Vec2 impulse = impulseList[i];
        const math::Vec2 ra = raList[i];
        const math::Vec2 rb = rbList[i];

        bodyA.linearVel += impulse * -bodyA.invMass;
        bodyA.angularVel += -bodyA.invInertia * math::Vec2::Cross(ra, impulse);
        bodyB.linearVel += impulse * bodyB.invMass;
        bodyB.angularVel += bodyB.invInertia * math::Vec2::Cross(rb, impulse);

        if (bodyA.bodyType != physics::RigidbodyType::Static)
            bodyA.AccumulateNormalImpulse(impulse.Negate());
        if (bodyB.bodyType != physics::RigidbodyType::Static)
            bodyB.AccumulateNormalImpulse(impulse);
    }

    // =========================================================================
    // PASS 2 — Friction impulses
    // ra/rb are recalculated here since velocities changed in Pass 1.
    // =========================================================================
    for (int i = 0; i < contactCount; i++)
    {
        // ── FIX: Coulomb's law requires a non-zero normal force.
        // If j == 0 the contact was skipped above (bodies separating) so
        // there is no normal force and therefore no friction.
        const float j = jList[i];
        if (j <= 0.0f)
            continue;

        const math::Vec2 ra = contactList[i] - bodyA.pos;
        const math::Vec2 rb = contactList[i] - bodyB.pos;
        raList[i] = ra;
        rbList[i] = rb;

        const math::Vec2 raPerp(-ra.y, ra.x);
        const math::Vec2 rbPerp(-rb.y, rb.x);

        const math::Vec2 relativeVelocity =
            (bodyB.linearVel + rbPerp * bodyB.angularVel) -
            (bodyA.linearVel + raPerp * bodyA.angularVel);

        math::Vec2 tangent =
            relativeVelocity - normal * math::Vec2::Dot(relativeVelocity, normal);

        // ── Stationary case ───────────────────────────────────────────────────
        // When relative velocity is zero the tangent direction is undefined and
        // the impulse solver produces nothing — but static friction IS acting and
        // must be displayed correctly for physics problems.
        //
        // Solution: compute static friction analytically from the gravitational
        // component along the contact surface.  This gives exactly the textbook
        // value  f_s = m·g·sin(θ)  capped at  μ_s · N  = μ_s · m·g·cos(θ).
        // No velocity change is applied because the body is already at rest.
        if (math::NearlyEqualVec(tangent, math::Vec2(0.0f, 0.0f)))
        {
            // ── Helper lambda ────────────────────────────────────────────────
            // Accumulates the analytical static-friction force for one body.
            // 'accumSign' applies Newton's 3rd law for the partner body.
            auto accumulateAnalyticStaticFriction =
                [&](physics::Rigidbody &body, float accumSign)
            {
                if (body.bodyType == physics::RigidbodyType::Static)
                    return;

                // Only engage the analytical path when truly at rest.
                // Fast-moving bodies that happen to have zero *relative* velocity
                // at this contact should take the kinetic path on the next frame.
                if (body.linearVel.Length() > kRestVelThreshold)
                    return;

                const float gPPM = PhysicsConstants::GRAVITY * SimulationConstants::PIXELS_PER_METER;
                const math::Vec2 gravForce(0.0f, -body.mass * gPPM);

                // Decompose gravity into the component perpendicular to the
                // surface (balanced by normal force) and the component parallel
                // to the surface (resisted by static friction).
                const math::Vec2 gravAlongNormal =
                    normal * math::Vec2::Dot(gravForce, normal);
                const math::Vec2 gravTangential = gravForce - gravAlongNormal;

                const float gravTangMag = gravTangential.Length();
                if (gravTangMag < 1e-4f)
                    return; // flat surface — no tangential gravity, no friction to show

                // N = |gravity · surface_normal| (magnitude only)
                const float normalForceMag =
                    std::abs(math::Vec2::Dot(gravForce, normal));

                // Coulomb cap: static friction ≤ μ_s × N
                const float frictionMag =
                    std::min(gravTangMag, staticFriction * normalForceMag);

                // Friction acts up the slope, opposing gravity's tendency to
                // slide the body downward along the surface.
                const math::Vec2 frictionForce =
                    gravTangential.Normalize().Negate() * frictionMag;

                // NOTE: AccumulateFrictionImpulse must use += in Rigidbody.cpp
                body.AccumulateFrictionImpulse(frictionForce * accumSign);
            };

            // bodyA is on the surface → friction acts on it directly (+1)
            // bodyB is the surface  → equal-and-opposite reaction        (-1)
            accumulateAnalyticStaticFriction(bodyA, 1.0f);
            accumulateAnalyticStaticFriction(bodyB, -1.0f);

            // Body is at rest — no velocity correction required
            continue;
        }

        // ── Kinetic / sliding case ────────────────────────────────────────────
        tangent = tangent.Normalize();

        const float raPerpDotT = math::Vec2::Dot(raPerp, tangent);
        const float rbPerpDotT = math::Vec2::Dot(rbPerp, tangent);
        const float denom = bodyA.invMass + bodyB.invMass + raPerpDotT * raPerpDotT * bodyA.invInertia + rbPerpDotT * rbPerpDotT * bodyB.invInertia;

        float jt = -math::Vec2::Dot(relativeVelocity, tangent);
        jt /= denom;
        jt /= static_cast<float>(contactCount);

        // Coulomb friction cone:
        //   |jt| ≤ μ_s·j  →  static  (just enough to stop sliding)
        //   |jt|  > μ_s·j  →  kinetic (f = μ_k·N)
        if (std::abs(jt) <= j * staticFriction)
            impulseFrictionList[i] = tangent * jt;
        else
            impulseFrictionList[i] = tangent * -(kineticFriction * j);
    }

    // Apply kinetic friction impulses + accumulate for FBD display
    // NOTE: AccumulateFrictionImpulse must use += (not =) in Rigidbody.cpp
    for (int i = 0; i < contactCount; i++)
    {
        const math::Vec2 impulseFriction = impulseFrictionList[i];

        // Skip contacts that had no friction (stationary case, or j == 0)
        if (math::NearlyEqualVec(impulseFriction, math::Vec2(0.0f, 0.0f)))
            continue;

        const math::Vec2 ra = raList[i];
        const math::Vec2 rb = rbList[i];

        bodyA.linearVel += impulseFriction * -bodyA.invMass;
        bodyA.angularVel += -bodyA.invInertia * math::Vec2::Cross(ra, impulseFriction);
        bodyB.linearVel += impulseFriction * bodyB.invMass;
        bodyB.angularVel += bodyB.invInertia * math::Vec2::Cross(rb, impulseFriction);

        if (bodyA.bodyType != physics::RigidbodyType::Static)
            bodyA.AccumulateFrictionImpulse(impulseFriction.Negate());
        if (bodyB.bodyType != physics::RigidbodyType::Static)
            bodyB.AccumulateFrictionImpulse(impulseFriction);
    }
}